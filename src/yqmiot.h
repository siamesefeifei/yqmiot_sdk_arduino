/*
The MIT License

Copyright (c) 2021, YQMIOT

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#include <functional>
#include <Arduino_JSON.h>
#include <ArduinoWebsockets.h>
#include <ESP8266WiFi.h>

/*
注意：
    登录失败时不重连，在服务器断开时重连。

!!服务器尚未实现连接优雅断开

客户端行为：
begin 是设置参数，并调用 connect，
调用 connect 后，客户端会不停的尝试保持与服务器的连接，直到 close 调用。
即便登录失败，依然做尝试。

*/

using namespace websockets;

#define YQMIOT_CMD_INVALID            0         // 无效命令
#define YQMIOT_CMD_RESPONSE           1         // 回复
#define YQMIOT_CMD_AUTH               2         // 认证
#define YQMIOT_CMD_READ               10        // 读取参数
#define YQMIOT_CMD_WRITE              11        // 写入参数
#define YQMIOT_CMD_NOTIFY             12        // 通知其他设备
// TODO 是否需要增加特征查询?
// TODO 增加批量读取

#define YQMIOT_TYPE_INVALID           0         // 无效类型
#define YQMIOT_TYPE_PROP              1         // 属性
#define YQMIOT_TYPE_CONFIG            2         // 配置
#define YQMIOT_TYPE_EVENT             3         // 事件
#define YQMIOT_TYPE_METHOD            4         // 方法

#define YQMIOT_NID_IOTSRV            0x7ffffe00 // 物联网主服务
#define YQMIOT_NID_NOTIFY            0x00000000 // Android IOS 通知推送服务
#define YQMIOT_NID_EMAIL             0x00000000 // 邮件服务
#define YQMIOT_NID_SMS               0x00000000 // 短信服务

#define YQMIOT_URL                   "ws://v1.yqmiot.com:27881/ws"

#define YQMIOT_STATE_CLOSED         0
#define YQMIOT_STATE_CONNECTING     1
#define YQMIOT_STATE_CONNECTED      2

#define YQMIOT_PRINTF Serial.printf

typedef std::function<void(int dst, int src, int id, int cmd, int hdl, const JSONVar& val)> YqmiotClientCallback;

class YqmiotClient {
public:
    YqmiotClient();

    ~YqmiotClient();

    // login_type 0 nid token 认证方式，1 token 认证方式
    void begin(int login_type, int nid, String token);

    void begin(int nid, String token);

    void begin(String token);

    void connect();

    void close();

    void _connect_async(int delay);

    void _send_login();

    void _on_data(WSInterfaceString data);

    void _on_event(WebsocketsEvent event, WSInterfaceString data);

    void _connect_retry();

    void loop();

    int get_state() const { return this->_state; }

    // 给指定设备发送指令
    void send_packet(int dst, int cmd, const JSONVar& val);
    void send_packet(int dst, int cmd, const JSONVar& val, int src); // 子网透传需要使用

    // 属性上报
    void report_props(const JSONVar& val);

    void write(int dst, int hdl, const JSONVar& val);

    void read(int dst, int hdl, const JSONVar& val);

    // 回复指定设备的调用
    void reply(int dst, int id, const JSONVar& val);

    // 读请求
    void on_read(YqmiotClientCallback callback) { _on_call_callback = callback; }

    // 写请求
    void on_write(YqmiotClientCallback callback) { _on_call_callback = callback; }

    // 其他设备通知 (事件，属性，变化)
    void on_notify(YqmiotClientCallback callback) { _on_call_callback = callback; }

    // 调用其他设备回复
    void on_response(YqmiotClientCallback callback) { _on_call_response_callback = callback; }

    void set_val(int hdl, const JSONVar& val) {
        _vals[hdl] = val;
    }

    JSONVar get_val(int hdl) {
        return _vals[hdl];
    }

private:
    WebsocketsClient _client;
    int _state;
    long _connect_timer;
    int _retry_count;

    YqmiotClientCallback _on_read_callback;
    YqmiotClientCallback _on_write_callback;
    YqmiotClientCallback _on_notify_callback;

    String _url;
    int _login_type;
    int _nid;
    String _token;
    int _pid;

    JSONVar _vals;
    YqmiotCharacteristic *_chars;
};

struct YqmiotCharacteristic
{
    int hdl;
    int typ;
    int flags;
    char *dec;
    JSONVar val;
};


extern YqmiotClient YQMIOT;