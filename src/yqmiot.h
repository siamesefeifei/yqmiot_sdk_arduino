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

#define YQMIOT_CMD_INVALID            0         // 无效命令 (invalid)
#define YQMIOT_CMD_RESPONSE           1         // 回复 (response)
#define YQMIOT_CMD_LOGIN              2         // 认证 (auth)
#define YQMIOT_CMD_CLOSE              3         // 断开 (close)
#define YQMIOT_CMD_PING               4         // ping
#define YQMIOT_CMD_PROPS_REPORT       5         // 属性上报 (props report)
#define YQMIOT_CMD_EVENT_REPORT       6         // 事件上报 (event report)
#define YQMIOT_CMD_CALL               7         // 服务调用 (service call)
#define YQMIOT_CMD_GET_CONFIG         8         // 读取配置
#define YQMIOT_CMD_SET_CONFIG         9         // 设置配置

#define YQMIOT_NID_IOTSRV            0x7ffffe00 // 物联网主服务
#define YQMIOT_NID_NOTIFY            0x00000000 // Android IOS 通知推送服务
#define YQMIOT_NID_EMAIL             0x00000000 // 邮件服务
#define YQMIOT_NID_SMS               0x00000000 // 短信服务

#define YQMIOT_URL                   "ws://v1.yqmiot.com:27881/ws"

#define YQMIOT_STATE_CLOSED         0
#define YQMIOT_STATE_CONNECTING     1
#define YQMIOT_STATE_CONNECTED      2

#define YQMIOT_PRINTF Serial.printf

typedef std::function<void(int dst, int src, int id, int cmd, const JSONVar& val)> YqmiotClientCallback;

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

    // 事件上报
    void report_event(const String& name, JSONVar& val);

    void report_event(const String& name);

    // 回复指定设备的调用
    void reply(int dst, int id, const JSONVar& val);

    // 服务调用指令
    void on_call(YqmiotClientCallback callback) { _on_call_callback = callback; }

    // 其他设备上报事件
    void on_event_report(YqmiotClientCallback callback) { _on_event_report_callback = callback; }

    // 其他设备上报属性
    void on_props_report(YqmiotClientCallback callback) { _on_props_report_callback = callback; }

    // 属性配置指令
    void on_set_config(YqmiotClientCallback callback) { _on_set_config_callback = callback; }

    // 属性读取指令
    void on_get_config(YqmiotClientCallback callback) { _on_get_config_callback = callback; }

    // 调用其他设备的回复
    void on_call_response(YqmiotClientCallback callback) { _on_call_response_callback = callback; }

    // 子网透传包
    void on_subnode_packet(YqmiotClientCallback callback) { _on_subnode_packet_callback = callback; }

private:
    WebsocketsClient _client;
    int _state;
    long _connect_timer;
    int _retry_count;

    YqmiotClientCallback _on_call_callback;
    YqmiotClientCallback _on_event_report_callback;
    YqmiotClientCallback _on_props_report_callback;
    YqmiotClientCallback _on_set_config_callback;
    YqmiotClientCallback _on_get_config_callback;
    YqmiotClientCallback _on_call_response_callback;
    YqmiotClientCallback _on_subnode_packet_callback;

    String _url;
    int _login_type;
    int _nid;
    String _token;
    int _pid;
};

extern YqmiotClient YQMIOT;