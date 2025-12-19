# MI E2EE（Windows / TCP Demo）

## 运行（Demo 模式：`mode=1`）

### 1) 启动服务端
- 目录示例：`server-win-Release.zip` 解压后（例如 `D:\core-test\1\s`）
- 确保同目录下有：
  - `mi_e2ee_server.exe`
  - `config.ini`
  - `test_user.txt`
- 启动：双击运行或命令行运行 `mi_e2ee_server.exe`（保持窗口不要关闭）

`test_user.txt` 格式：每行 `username:password`，支持整行 `#` 注释与行内注释（`空格 + #`）。

示例（构建产物里已自带一份）：
```
u1:p1
alice:alice123
bob:bob123
```

### 2) 启动客户端 UI
- 目录示例：`client-ui-Release.zip` 解压后（例如 `D:\core-test\1\c`）
- 确保同目录下有 `client_config.ini`（或 `config.ini`），内容：
```
[client]
server_ip=127.0.0.1
server_port=9000
```
- 运行 `mi_e2ee.exe`，用 `test_user.txt` 里的账号密码登录。

### 3) 好友列表 / 添加好友（当前实现）
- 登录后主列表会向后端拉取好友列表（为空会提示“暂无好友”）。
- 点击右上角 `+` 输入好友账号（必须在认证用户表里存在），后端会直接建立**双向好友关系**，列表立刻出现该好友。
- 好友备注：列表右键 `修改备注`（为空则显示账号）。
- 说明：
  - `mode=1`（demo）：好友关系在服务端内存中保存，服务端重启会清空。
  - `mode=0`（mysql）：好友关系写入 MySQL 表 `user_friend`，服务端重启不丢失。

## MySQL 登录（`mode=0`）建库建表示例

后端查询语句固定为：
`SELECT password FROM user_auth WHERE username=? LIMIT 1`

### 1) 建库建表
```sql
CREATE DATABASE IF NOT EXISTS mi_e2ee
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_bin;

USE mi_e2ee;

CREATE TABLE IF NOT EXISTS user_auth (
  username VARCHAR(64) NOT NULL,
  password VARCHAR(255) NOT NULL,
  PRIMARY KEY (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

-- 好友关系（双向各一行）
CREATE TABLE IF NOT EXISTS user_friend (
  username VARCHAR(64) NOT NULL,
  friend_username VARCHAR(64) NOT NULL,
  remark VARCHAR(128) NOT NULL DEFAULT '',
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (username, friend_username),
  INDEX idx_friend_username(friend_username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
```

### 2) 插入示例账号
后端支持 3 种密码存储格式：
1) 明文：`password = 'alice123'`
2) 纯哈希：`password = sha256(password)` 的十六进制（**小写**）
3) 加盐：`password = 'salt:sha256(salt+password)'`（**小写**）

```sql
-- 1) 明文
INSERT INTO user_auth(username, password) VALUES ('alice', 'alice123');

-- 2) sha256(password)（注意转小写，后端内部计算为小写 hex）
INSERT INTO user_auth(username, password)
VALUES ('bob', LOWER(SHA2('bob123', 256)));

-- 3) salt:sha256(salt+password)
SET @salt = 's1';
INSERT INTO user_auth(username, password)
VALUES ('u1', CONCAT(@salt, ':', LOWER(SHA2(CONCAT(@salt, 'p1'), 256))));
```

## 加好友流程建议

当前实现是“直接加好友”。更接近主流聊天软件的交互可以拆成：
1) 搜索/输入账号 → 发送好友申请（pending）
2) 对方“新的好友”列表显示申请 → 同意/拒绝
3) 同意后双方写入好友关系，好友列表拉取即可展示

（如需我把“申请/同意/拒绝 + MySQL 持久化 + UI 展示”也做掉，告诉我期望交互与表结构即可。）
