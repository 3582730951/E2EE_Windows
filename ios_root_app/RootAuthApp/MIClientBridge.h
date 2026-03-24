#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface MIClientBridge : NSObject

@property (nonatomic, readonly, getter=isReady) BOOL ready;
@property (nonatomic, copy, readonly) NSString *configPath;
@property (nonatomic, copy, readonly) NSString *lastCreateError;
@property (nonatomic, copy, readonly) NSString *lastError;
@property (nonatomic, copy, readonly) NSString *token;
@property (nonatomic, copy, readonly) NSString *deviceID;
@property (nonatomic, copy, readonly) NSString *deviceDisplayID;
@property (nonatomic, readonly) BOOL remoteOK;
@property (nonatomic, copy, readonly) NSString *remoteError;

- (BOOL)openWithConfigPath:(NSString *)configPath;
- (void)close;

- (BOOL)createAccountWithUsername:(NSString *)username
                         password:(NSString *)password;
- (BOOL)loginWithUsername:(NSString *)username
                 password:(NSString *)password;
- (BOOL)loginWithUsername:(NSString *)username
                 password:(NSString *)password
                 rootCode:(NSString *)rootCode;
- (BOOL)bootstrapRootAuthWithPublicKeyHex:(NSString *)publicKeyHex;
- (BOOL)publishPrekeys;
- (BOOL)heartbeat;

- (NSArray<NSDictionary<NSString *, id> *> *)pollEventsWithMaxCount:(NSInteger)maxCount
                                                             waitMS:(NSInteger)waitMS;
- (NSArray<NSDictionary<NSString *, id> *> *)loadHistoryForConversationID:(NSString *)conversationID
                                                                   isGroup:(BOOL)isGroup
                                                                     limit:(NSInteger)limit;
- (nullable NSString *)sendPrivateText:(NSString *)text
                        toPeerUsername:(NSString *)peerUsername;
- (nullable NSString *)sendGroupText:(NSString *)text
                              groupID:(NSString *)groupID;
- (NSArray<NSDictionary<NSString *, id> *> *)listFriends;
- (NSArray<NSDictionary<NSString *, id> *> *)listDevices;
- (nullable NSString *)createGroup;
- (BOOL)joinGroupWithID:(NSString *)groupID;
- (BOOL)leaveGroupWithID:(NSString *)groupID;
- (nullable NSString *)sendGroupInviteToPeerUsername:(NSString *)peerUsername
                                             groupID:(NSString *)groupID;
- (NSArray<NSDictionary<NSString *, id> *> *)listGroupMembersForGroupID:(NSString *)groupID;

@end

NS_ASSUME_NONNULL_END
