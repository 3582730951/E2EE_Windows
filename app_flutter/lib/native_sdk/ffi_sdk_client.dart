import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import '../domain/entities/models.dart';
import 'ffi_sdk_projection.dart';
import 'sdk_client.dart';
import 'sdk_ffi_extra_bindings.dart';
import 'sdk_ffi_bindings.dart';

class FfiSdkClient implements NativeSdkClient {
  FfiSdkClient(this.libraryPath);

  static const int _eventBatchSize = 32;
  static const int _historyBatchSize = 64;
  static const int _listBatchSize = 64;
  static const int _historySeedCount = 6;
  static const Duration _pollInterval = Duration(milliseconds: 600);
  static const Duration _heartbeatInterval = Duration(seconds: 20);

  final String libraryPath;

  final StreamController<List<ConversationSummary>> _conversationController =
      StreamController<List<ConversationSummary>>.broadcast();
  final Map<String, StreamController<List<ChatMessage>>> _messageControllers =
      <String, StreamController<List<ChatMessage>>>{};
  final StreamController<List<ContactProfile>> _contactController =
      StreamController<List<ContactProfile>>.broadcast();
  final StreamController<List<DeviceTrustInfo>> _deviceController =
      StreamController<List<DeviceTrustInfo>>.broadcast();

  late final DynamicLibrary _library;
  late final MiClientBindings _bindings;
  late final MiClientExtraBindings _extra;
  Pointer<Void> _handle = nullptr;

  bool _initialized = false;
  bool _authenticated = false;
  bool _polling = false;
  Future<void> _nativeTail = Future<void>.value();

  Timer? _pollTimer;
  Timer? _heartbeatTimer;

  String _currentDeviceId = '';
  String _currentDeviceDisplayId = '';

  List<ConversationSummary> _conversations = const <ConversationSummary>[];
  final Map<String, ConversationSummary> _conversationIndex =
      <String, ConversationSummary>{};
  final Map<String, List<ChatMessage>> _messages =
      <String, List<ChatMessage>>{};
  final Set<String> _loadedHistoryIds = <String>{};

  List<ContactProfile> _contacts = const <ContactProfile>[];
  final Map<String, ContactProfile> _contactsById = <String, ContactProfile>{};
  final Map<String, SdkFriendSnapshot> _friendSnapshots =
      <String, SdkFriendSnapshot>{};
  final Map<String, bool> _presenceByUser = <String, bool>{};

  List<DeviceTrustInfo> _devices = const <DeviceTrustInfo>[];

  @override
  Future<void> initialize() async {
    if (_initialized) {
      return;
    }
    _library = _openSdkLibrary();
    _bindings = MiClientBindings(_library);
    _extra = MiClientExtraBindings(_library);

    final configPath = Platform.environment['MI_E2EE_CLIENT_CONFIG'];
    _handle = await _withNative(() {
      final configPtr = (configPath != null && configPath.isNotEmpty)
          ? configPath.toNativeUtf8()
          : nullptr.cast<Utf8>();
      try {
        return _bindings.create(configPtr);
      } finally {
        if (configPtr != nullptr.cast<Utf8>()) {
          calloc.free(configPtr);
        }
      }
    });
    if (_handle == nullptr) {
      throw StateError(_readString(_bindings.lastCreateError()));
    }

    _initialized = true;
    _emitContacts();
    _emitDevices();
    _emitConversations();
  }

  @override
  Future<SessionProfile?> login({
    required String username,
    required String password,
  }) async {
    await initialize();
    final trimmedUsername = username.trim();
    final trimmedPassword = password.trim();
    if (trimmedUsername.isEmpty || trimmedPassword.isEmpty) {
      return null;
    }

    final ok = await _withNative(() {
      final usernamePtr = trimmedUsername.toNativeUtf8();
      final passwordPtr = trimmedPassword.toNativeUtf8();
      try {
        return _bindings.login(_handle, usernamePtr, passwordPtr) == 1;
      } finally {
        calloc.free(usernamePtr);
        calloc.free(passwordPtr);
      }
    });
    if (!ok) {
      final reason = await _withNative(_currentError);
      if (reason.isEmpty) {
        return null;
      }
      throw StateError(reason);
    }

    _authenticated = true;
    final deviceIds = await _withNative(
      () => (
        deviceId: _readString(_bindings.deviceId(_handle)),
        displayId: _readString(_bindings.deviceDisplayId(_handle)),
      ),
    );
    _currentDeviceId = deviceIds.deviceId;
    _currentDeviceDisplayId = deviceIds.displayId;

    await _reloadContacts();
    await _reloadDevices();
    await _loadRecentHistorySnapshot();
    _seedDirectConversations();
    for (final contact in _contacts.take(_historySeedCount)) {
      await _ensureHistoryLoaded(contact.id);
    }
    _startBackgroundSync();

    final subtitle = _currentDeviceDisplayId.isEmpty
        ? '已接入原生 SDK'
        : '$_currentDeviceDisplayId · 已接入原生 SDK';
    return SessionProfile(
      username: trimmedUsername,
      displayName: trimmedUsername,
      subtitle: subtitle,
    );
  }

  @override
  Future<void> logout() async {
    _stopBackgroundSync();
    if (_handle != nullptr && _authenticated) {
      await _withNative(() => _bindings.logout(_handle));
    }
    _authenticated = false;
    _currentDeviceId = '';
    _currentDeviceDisplayId = '';
    _loadedHistoryIds.clear();
    _presenceByUser.clear();
    _friendSnapshots.clear();
    _contactsById.clear();
    _conversationIndex.clear();
    _messages.clear();
    _contacts = const <ContactProfile>[];
    _devices = const <DeviceTrustInfo>[];
    _conversations = const <ConversationSummary>[];
    _emitContacts();
    _emitDevices();
    _emitConversations();
    for (final controller in _messageControllers.values) {
      controller.add(const <ChatMessage>[]);
    }
  }

  @override
  Stream<List<ConversationSummary>> watchConversations() async* {
    yield _conversations;
    yield* _conversationController.stream;
  }

  @override
  Stream<List<ChatMessage>> watchMessages(String conversationId) async* {
    await _ensureHistoryLoaded(conversationId);
    yield List<ChatMessage>.unmodifiable(
      _messages[conversationId] ?? const <ChatMessage>[],
    );
    yield* _controllerFor(conversationId).stream;
  }

  @override
  Stream<List<ContactProfile>> watchContacts() async* {
    yield _contacts;
    yield* _contactController.stream;
  }

  @override
  Stream<List<DeviceTrustInfo>> watchDevices() async* {
    yield _devices;
    yield* _deviceController.stream;
  }

  @override
  Future<void> markConversationRead({required String conversationId}) async {
    await _ensureHistoryLoaded(conversationId);

    final existingMessages = _messages[conversationId];
    if (existingMessages != null) {
      final updatedMessages = existingMessages
          .map(
            (message) =>
                message.direction == MessageDirection.incoming &&
                    message.status != MessageDeliveryStatus.read
                ? _copyMessageWithStatus(message, MessageDeliveryStatus.read)
                : message,
          )
          .toList();
      _messages[conversationId] = updatedMessages;
      _controllerFor(
        conversationId,
      ).add(List<ChatMessage>.unmodifiable(updatedMessages));
    }

    final existingSummary = _conversationIndex[conversationId];
    if (existingSummary != null) {
      _conversationIndex[conversationId] = existingSummary.copyWith(
        unreadCount: 0,
        mentionCount: 0,
        isTyping: false,
      );
      _emitConversations();
    }
  }

  @override
  Future<void> sendMessage({
    required String conversationId,
    required String text,
  }) async {
    final trimmed = text.trim();
    if (!_authenticated || trimmed.isEmpty || _handle == nullptr) {
      return;
    }

    final conversationKey = sdkConversationKey(conversationId);
    final result = await _withNative(() {
      final targetPtr = conversationKey.toNativeUtf8();
      final textPtr = trimmed.toNativeUtf8();
      final outMessageId = calloc<Pointer<Utf8>>();
      try {
        final ok = isGroupConversationId(conversationId)
            ? _bindings.sendGroupText(_handle, targetPtr, textPtr, outMessageId)
            : _bindings.sendPrivateText(
                _handle,
                targetPtr,
                textPtr,
                outMessageId,
              );
        return (ok: ok == 1, messageId: _takeOwnedString(outMessageId));
      } finally {
        calloc.free(outMessageId);
        calloc.free(targetPtr);
        calloc.free(textPtr);
      }
    });
    {
      final timestamp = DateTime.now();
      if (result.ok) {
        _upsertMessage(
          ChatMessage(
            id: result.messageId.isEmpty
                ? 'local-${timestamp.microsecondsSinceEpoch}'
                : result.messageId,
            conversationId: conversationId,
            senderLabel: '你',
            text: trimmed,
            timestamp: timestamp,
            direction: MessageDirection.outgoing,
            status: MessageDeliveryStatus.sent,
          ),
        );
        _refreshConversationSummary(
          conversationId,
          isGroup: isGroupConversationId(conversationId),
        );
        _emitConversations();
        return;
      }

      _upsertMessage(
        ChatMessage(
          id: 'failed-${timestamp.microsecondsSinceEpoch}',
          conversationId: conversationId,
          senderLabel: '你',
          text: trimmed,
          timestamp: timestamp,
          direction: MessageDirection.outgoing,
          status: MessageDeliveryStatus.failed,
        ),
      );
      _refreshConversationSummary(
        conversationId,
        isGroup: isGroupConversationId(conversationId),
      );
      _emitConversations();
    }
  }

  @override
  void dispose() {
    _stopBackgroundSync();
    for (final controller in _messageControllers.values) {
      controller.close();
    }
    _conversationController.close();
    _contactController.close();
    _deviceController.close();
    if (_handle != nullptr) {
      _bindings.destroy(_handle);
      _handle = nullptr;
    }
  }

  Future<void> _reloadContacts() async {
    if (!_authenticated || _handle == nullptr) {
      return;
    }
    final friends = await _withNative(_readFriends);
    _friendSnapshots
      ..clear()
      ..addEntries(
        friends.map(
          (friend) =>
              MapEntry<String, SdkFriendSnapshot>(friend.username, friend),
        ),
      );
    _contacts =
        friends
            .map(
              (friend) => contactFromFriend(
                friend,
                online: _presenceByUser[friend.username] ?? false,
              ),
            )
            .toList()
          ..sort((left, right) {
            if (left.online != right.online) {
              return right.online ? 1 : -1;
            }
            return left.displayName.compareTo(right.displayName);
          });
    _contactsById
      ..clear()
      ..addEntries(
        _contacts.map(
          (contact) => MapEntry<String, ContactProfile>(contact.id, contact),
        ),
      );
    _emitContacts();
  }

  Future<void> _reloadDevices() async {
    if (!_authenticated || _handle == nullptr) {
      return;
    }
    final devices = await _withNative(_readDevices);
    final now = DateTime.now();
    _devices =
        devices
            .map(
              (device) => deviceFromSnapshot(
                device,
                currentDeviceId: _currentDeviceId,
                now: now,
              ),
            )
            .toList()
          ..sort((left, right) {
            if (left.trustLabel == right.trustLabel) {
              return left.name.compareTo(right.name);
            }
            return left.trustLabel == '当前设备' ? -1 : 1;
          });
    _emitDevices();
  }

  void _seedDirectConversations() {
    for (final contact in _contacts) {
      _refreshConversationSummary(
        contact.id,
        title: contact.displayName,
        isGroup: false,
        isOnline: contact.online,
      );
    }
    _emitConversations();
  }

  Future<void> _loadRecentHistorySnapshot() async {
    if (!_authenticated || _handle == nullptr) {
      return;
    }
    final entries = await _withNative(() {
      final capacity = _historySeedCount * _historyBatchSize;
      final buffer = calloc<MiHistoryEntry>(capacity);
      try {
        final count = _extra.exportRecentHistorySnapshot(
          _handle,
          _historySeedCount,
          _historyBatchSize,
          buffer,
          capacity,
        );
        return List<SdkHistorySnapshot>.generate(
          count,
          (index) => _snapshotFromHistoryEntry(buffer[index]),
        );
      } finally {
        calloc.free(buffer);
      }
    });

    final grouped = <String, List<ChatMessage>>{};
    for (final entry in entries) {
      final message = historyToMessage(entry);
      grouped
          .putIfAbsent(message.conversationId, () => <ChatMessage>[])
          .add(message);
    }
    for (final item in grouped.entries) {
      item.value.sort(
        (left, right) => left.timestamp.compareTo(right.timestamp),
      );
      _messages[item.key] = item.value;
      _loadedHistoryIds.add(item.key);
      _controllerFor(item.key).add(List<ChatMessage>.unmodifiable(item.value));
      _refreshConversationSummary(
        item.key,
        isGroup: isGroupConversationId(item.key),
      );
    }
    if (grouped.isNotEmpty) {
      _emitConversations();
    }
  }

  Future<void> _ensureHistoryLoaded(String conversationId) async {
    if (!_authenticated ||
        _handle == nullptr ||
        !_loadedHistoryIds.add(conversationId)) {
      return;
    }

    final conversationKey = sdkConversationKey(conversationId);
    final messages = await _withNative(() {
      final convPtr = conversationKey.toNativeUtf8();
      final entries = calloc<MiHistoryEntry>(_historyBatchSize);
      try {
        final count = _bindings.loadChatHistory(
          _handle,
          convPtr,
          isGroupConversationId(conversationId) ? 1 : 0,
          _historyBatchSize,
          entries,
          _historyBatchSize,
        );
        return List<ChatMessage>.generate(
          count,
          (index) =>
              historyToMessage(_snapshotFromHistoryEntry(entries[index])),
        )..sort((left, right) => left.timestamp.compareTo(right.timestamp));
      } finally {
        calloc.free(entries);
        calloc.free(convPtr);
      }
    });
    _messages[conversationId] = messages;
    _controllerFor(
      conversationId,
    ).add(List<ChatMessage>.unmodifiable(messages));
    _refreshConversationSummary(
      conversationId,
      isGroup: isGroupConversationId(conversationId),
    );
    _emitConversations();
  }

  void _startBackgroundSync() {
    _stopBackgroundSync();
    _pollTimer = Timer.periodic(_pollInterval, (_) => _pollEvents());
    _heartbeatTimer = Timer.periodic(_heartbeatInterval, (_) {
      if (_authenticated && _handle != nullptr) {
        unawaited(_withNative(() => _bindings.heartbeat(_handle)));
      }
    });
    unawaited(_pollEvents());
  }

  void _stopBackgroundSync() {
    _pollTimer?.cancel();
    _heartbeatTimer?.cancel();
    _pollTimer = null;
    _heartbeatTimer = null;
  }

  Future<void> _pollEvents() async {
    if (_polling || !_authenticated || _handle == nullptr) {
      return;
    }
    _polling = true;
    try {
      final snapshots = await _withNative(() {
        final events = calloc<MiEvent>(_eventBatchSize);
        try {
          final count = _bindings.pollEvent(
            _handle,
            events,
            _eventBatchSize,
            0,
          );
          return List<SdkEventSnapshot>.generate(
            count,
            (index) => _snapshotFromEvent(events[index]),
          );
        } finally {
          calloc.free(events);
        }
      });
      for (final event in snapshots) {
        _applyEvent(event);
      }
    } finally {
      _polling = false;
    }
  }

  void _applyEvent(SdkEventSnapshot event) {
    switch (event.type) {
      case SdkEventType.delivery:
        _updateMessageStatus(
          directConversationId(event.peer),
          event.messageId,
          MessageDeliveryStatus.delivered,
        );
        return;
      case SdkEventType.readReceipt:
        _updateMessageStatus(
          directConversationId(event.peer),
          event.messageId,
          MessageDeliveryStatus.read,
        );
        return;
      case SdkEventType.typing:
        _refreshPresence(event.peer, typing: event.typing);
        return;
      case SdkEventType.presence:
        _refreshPresence(event.peer, online: event.online);
        return;
      default:
        final projected = eventToMessage(event);
        if (projected == null) {
          return;
        }
        _upsertMessage(projected.message);
        final conversationId = projected.message.conversationId;
        _refreshConversationSummary(
          conversationId,
          isGroup: projected.isGroup,
          isTyping: false,
        );
        _emitConversations();
        return;
    }
  }

  void _refreshPresence(String username, {bool? online, bool? typing}) {
    if (online != null) {
      _presenceByUser[username] = online;
      final snapshot = _friendSnapshots[username];
      if (snapshot != null) {
        final contact = contactFromFriend(snapshot, online: online);
        _contactsById[username] = contact;
        _contacts = _contactsById.values.toList()
          ..sort((left, right) {
            if (left.online != right.online) {
              return right.online ? 1 : -1;
            }
            return left.displayName.compareTo(right.displayName);
          });
        _emitContacts();
      }
    }

    final conversationId = directConversationId(username);
    _refreshConversationSummary(
      conversationId,
      isGroup: false,
      isOnline: _presenceByUser[username] ?? false,
      isTyping: typing,
    );
    _emitConversations();
  }

  void _updateMessageStatus(
    String conversationId,
    String messageId,
    MessageDeliveryStatus status,
  ) {
    final existing = _messages[conversationId];
    if (existing == null || existing.isEmpty) {
      return;
    }
    final updated = existing
        .map(
          (message) => message.id == messageId
              ? _copyMessageWithStatus(message, status)
              : message,
        )
        .toList();
    _messages[conversationId] = updated;
    _controllerFor(conversationId).add(List<ChatMessage>.unmodifiable(updated));
    _refreshConversationSummary(
      conversationId,
      isGroup: isGroupConversationId(conversationId),
    );
    _emitConversations();
  }

  void _upsertMessage(ChatMessage message) {
    final existing = List<ChatMessage>.from(
      _messages[message.conversationId] ?? const <ChatMessage>[],
    );
    final index = existing.indexWhere(
      (candidate) => candidate.id == message.id,
    );
    if (index >= 0) {
      existing[index] = message;
    } else {
      existing.add(message);
    }
    existing.sort((left, right) => left.timestamp.compareTo(right.timestamp));
    _messages[message.conversationId] = existing;
    _controllerFor(
      message.conversationId,
    ).add(List<ChatMessage>.unmodifiable(existing));
  }

  ChatMessage _copyMessageWithStatus(
    ChatMessage message,
    MessageDeliveryStatus status,
  ) {
    return message.copyWith(status: status);
  }

  void _refreshConversationSummary(
    String conversationId, {
    String? title,
    required bool isGroup,
    bool? isOnline,
    bool? isTyping,
  }) {
    final existing = _conversationIndex[conversationId];
    _conversationIndex[conversationId] = buildConversationSummary(
      conversationId: conversationId,
      title: title ?? existing?.title ?? _conversationTitle(conversationId),
      messages: _messages[conversationId] ?? const <ChatMessage>[],
      isGroup: isGroup,
      isOnline:
          isOnline ??
          existing?.isOnline ??
          _presenceByUser[conversationId] ??
          false,
      isTyping: isTyping ?? existing?.isTyping ?? false,
    );
  }

  String _conversationTitle(String conversationId) {
    if (isGroupConversationId(conversationId)) {
      final groupId = sdkConversationKey(conversationId);
      return '群聊 $groupId';
    }
    return _contactsById[conversationId]?.displayName ?? conversationId;
  }

  List<SdkFriendSnapshot> _readFriends() {
    var capacity = _listBatchSize;
    while (true) {
      final entries = calloc<MiFriendEntry>(capacity);
      try {
        final count = _bindings.listFriends(_handle, entries, capacity);
        final friends = List<SdkFriendSnapshot>.generate(
          count,
          (index) => SdkFriendSnapshot(
            username: _readString(entries[index].username),
            remark: _readString(entries[index].remark),
          ),
        );
        if (count < capacity || capacity >= 1024) {
          return friends;
        }
      } finally {
        calloc.free(entries);
      }
      capacity *= 2;
    }
  }

  List<SdkDeviceSnapshot> _readDevices() {
    var capacity = _listBatchSize;
    while (true) {
      final entries = calloc<MiDeviceEntry>(capacity);
      try {
        final count = _bindings.listDevices(_handle, entries, capacity);
        final devices = List<SdkDeviceSnapshot>.generate(
          count,
          (index) => SdkDeviceSnapshot(
            deviceId: _readString(entries[index].deviceId),
            displayId: _readString(entries[index].displayId),
            lastSeenSec: entries[index].lastSeenSec,
          ),
        );
        if (count < capacity || capacity >= 1024) {
          return devices;
        }
      } finally {
        calloc.free(entries);
      }
      capacity *= 2;
    }
  }

  StreamController<List<ChatMessage>> _controllerFor(String conversationId) {
    return _messageControllers.putIfAbsent(
      conversationId,
      () => StreamController<List<ChatMessage>>.broadcast(),
    );
  }

  void _emitConversations() {
    _conversations = _conversationIndex.values.toList()
      ..sort((left, right) {
        final tsCompare = right.lastUpdated.compareTo(left.lastUpdated);
        if (tsCompare != 0) {
          return tsCompare;
        }
        return left.title.compareTo(right.title);
      });
    _conversationController.add(
      List<ConversationSummary>.unmodifiable(_conversations),
    );
  }

  void _emitContacts() {
    _contactController.add(List<ContactProfile>.unmodifiable(_contacts));
  }

  void _emitDevices() {
    _deviceController.add(List<DeviceTrustInfo>.unmodifiable(_devices));
  }

  String _currentError() {
    final lastError = _readString(_bindings.lastError(_handle));
    if (lastError.isNotEmpty) {
      return lastError;
    }
    return _readString(_bindings.remoteError(_handle));
  }

  String _takeOwnedString(Pointer<Pointer<Utf8>> buffer) {
    final value = buffer.value;
    if (value == nullptr) {
      return '';
    }
    try {
      return value.toDartString();
    } finally {
      _bindings.free(value.cast<Void>());
      buffer.value = nullptr;
    }
  }

  DynamicLibrary _openSdkLibrary() {
    if (Platform.isIOS) {
      return DynamicLibrary.process();
    }
    if (Platform.isAndroid) {
      return DynamicLibrary.open(
        libraryPath.isEmpty ? 'libmi_e2ee_client_sdk.so' : libraryPath,
      );
    }
    if (Platform.isMacOS) {
      return DynamicLibrary.open(
        libraryPath.isEmpty ? 'libmi_e2ee_client_sdk.dylib' : libraryPath,
      );
    }
    if (Platform.isLinux) {
      return DynamicLibrary.open(
        libraryPath.isEmpty ? 'libmi_e2ee_client_sdk.so' : libraryPath,
      );
    }
    return DynamicLibrary.open(
      libraryPath.isEmpty ? 'mi_e2ee_client_sdk.dll' : libraryPath,
    );
  }

  Future<T> _withNative<T>(T Function() action) {
    final previous = _nativeTail.catchError((_) {});
    final completer = Completer<T>();
    _nativeTail = previous.then<void>((_) {
      try {
        completer.complete(action());
      } catch (error, stackTrace) {
        completer.completeError(error, stackTrace);
      }
    });
    return completer.future;
  }

  SdkHistorySnapshot _snapshotFromHistoryEntry(MiHistoryEntry entry) {
    return SdkHistorySnapshot(
      kind: entry.kind,
      status: entry.status,
      isGroup: entry.isGroup != 0,
      outgoing: entry.outgoing != 0,
      timestampSec: entry.timestampSec,
      conversationKey: _readString(entry.convId),
      sender: _readString(entry.sender),
      messageId: _readString(entry.messageId),
      text: _readString(entry.text),
      fileId: _readString(entry.fileId),
      fileKey: _readBytes(entry.fileKey, entry.fileKeyLen),
      fileName: _readString(entry.fileName),
      fileSize: entry.fileSize,
      stickerId: _readString(entry.stickerId),
      canonicalEnvelope:
          _readBytes(entry.canonicalEnvelope, entry.canonicalEnvelopeLen),
      messageType: entry.messageType,
    );
  }

  SdkEventSnapshot _snapshotFromEvent(MiEvent event) {
    return SdkEventSnapshot(
      type: event.type,
      timestampMs: event.tsMs,
      peer: _readString(event.peer),
      sender: _readString(event.sender),
      groupId: _readString(event.groupId),
      messageId: _readString(event.messageId),
      text: _readString(event.text),
      fileId: _readString(event.fileId),
      fileKey: _readBytes(event.fileKey, event.fileKeyLen),
      fileName: _readString(event.fileName),
      fileSize: event.fileSize,
      stickerId: _readString(event.stickerId),
      typing: event.typing != 0,
      online: event.online != 0,
      callId: _readCallId(event.callId),
      callKeyId: event.callKeyId,
      callOp: event.callOp,
      callMediaFlags: event.callMediaFlags,
      payload: _readBytes(event.payload, event.payloadLen),
    );
  }

  Uint8List? _readBytes(Pointer<Uint8> pointer, int length) {
    if (pointer == nullptr || length <= 0) {
      return null;
    }
    return Uint8List.fromList(pointer.asTypedList(length));
  }

  Uint8List _readCallId(Array<Uint8> value) {
    final out = Uint8List(16);
    for (var index = 0; index < out.length; index += 1) {
      out[index] = value[index];
    }
    return out;
  }

  String _readString(Pointer<Utf8> pointer) {
    if (pointer == nullptr) {
      return '';
    }
    return pointer.toDartString();
  }
}
