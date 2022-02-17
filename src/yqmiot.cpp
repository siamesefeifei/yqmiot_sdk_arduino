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
#include "yqmiot.h"

YqmiotClient YQMIOT;

YqmiotClient::YqmiotClient() {
    this->_state = YQMIOT_STATE_CLOSED;
    this->_connect_timer = 0;
    this->_retry_count = 0;
    this->_login_type = 0;
    this->_nid = 0;
    this->_pid = 0;

    this->_client.onMessage([this](WebsocketsClient&, WebsocketsMessage msg) {
      this->_on_data(msg.data());
    });
    this->_client.onEvent([this](WebsocketsClient&, WebsocketsEvent event, WSInterfaceString data) {
        this->_on_event(event, data);
    });
}

YqmiotClient::~YqmiotClient() {

}

void YqmiotClient::begin(int nid, String token) {
    this->begin(0, nid, token);
}

void YqmiotClient::begin(String token) {
    this->begin(1, 0, token);
}

void YqmiotClient::begin(int login_type, int nid, String token) {
    // 只有停止模式下才能配置参数
    if (this->_state == YQMIOT_STATE_CLOSED) {
        this->_url = YQMIOT_URL;
        this->_login_type = login_type;
        this->_nid = nid;
        this->_token = token;

        YQMIOT_PRINTF("[YqmiotClient] begin, url: %s, login_type: %d, nid: %d, token: %s\n",
            this->_url.c_str(), login_type, nid, token.c_str());
        this->connect();
    }
}

void YqmiotClient::connect() {
    if (this->_state == YQMIOT_STATE_CLOSED) {
        this->_state = YQMIOT_STATE_CONNECTING;
        this->_retry_count = 0;
        this->_connect_async(0);
    }
}

// 统一使用异步调用策略
void YqmiotClient::_connect_async(int delay) {
    this->_connect_timer = millis() + delay;
}

void YqmiotClient::_connect_retry() {
    int delay = min(1 << this->_retry_count++, 30) * 1000;
    this->_connect_async(delay);
    YQMIOT_PRINTF("[YqmiotClient] wait for %dms retry!\n", delay);
}

void YqmiotClient::close() {
    if (this->_state == YQMIOT_STATE_CONNECTING) {
        this->_client.close();
        this->_state = YQMIOT_STATE_CLOSED;
    } else if (this->_state == YQMIOT_STATE_CONNECTED) {
        this->_client.close();
        this->_state = YQMIOT_STATE_CLOSED;
    }
}

void YqmiotClient::send_packet(int dst, int cmd, const JSONVar& val) {
    if (this->_state != YQMIOT_STATE_CONNECTED) {
        return;
    }

    JSONVar p;
    p["d"] = dst;
    p["c"] = cmd;
    p["v"] = val;
    String data = JSON.stringify(p);
    this->_client.send(data);
}

void YqmiotClient::send_packet(int dst, int cmd, const JSONVar& val, int src) {
    if (this->_state != YQMIOT_STATE_CONNECTED) {
        return;
    }
    
    JSONVar p;
    p["d"] = dst;
    p["s"] = src;
    p["c"] = cmd;
    p["v"] = val;
    String data = JSON.stringify(p);
    this->_client.send(data);
}

void YqmiotClient::loop() {
    if (this->_connect_timer > 0
        && this->_connect_timer <= millis()) {
        this->_connect_timer = 0;

        YQMIOT_PRINTF("[YqmiotClient] connect to server ...\n");
        // XXX 实现异步连接
        if (!this->_client.connect(this->_url)) {
            YQMIOT_PRINTF("[YqmiotClient] connect failed!\n");
            this->_connect_retry();
        }
    }
    this->_client.poll();
}

void YqmiotClient::_on_data(WSInterfaceString data) {
    JSONVar p = JSON.parse(data);
    if (JSON.typeof(p) != "object") {
        YQMIOT_PRINTF("[YqmiotClient] recv data error! %s\n", data.c_str());
        return;
    }
YQMIOT_PRINTF("YqmiotClient::_on_data %s", data.c_str());
    int dst = (int)p["d"];
    int src = (int)p["s"];
    int id = (int)p["i"];
    int cmd = (int)p["c"];
    int hdl = (int)p["h"];
    JSONVar val = p["v"];

    // XXX login_type == 1 时无法获取 nid
    // if (dst != this->_nid) {
    //     if (this->_on_subnode_packet_callback) {
    //         this->_on_subnode_packet_callback(dst, src, id, cmd, val);
    //     }
    //     return;
    // }

    switch (cmd) {
    case YQMIOT_CMD_CALL:
        if (this->_on_call_callback) {
            this->_on_call_callback(dst, src, id, cmd, val);
        }
        break;

    case YQMIOT_CMD_RESPONSE:
        // XXX 处理绕回问题
        if (id == 0xffff) {
            // 登陆回包
            int err = (int)val["$err"];
            const char* msg = (const char*)val["$msg"];
            if (err == 0) {
                YQMIOT_PRINTF("[YqmiotClient] login success!\n");
                this->_state = YQMIOT_STATE_CONNECTED;
            } else {
                YQMIOT_PRINTF("[YqmiotClient] login failed, err: %d, msg: %s\n", err, msg ? msg : "");
            }
            return;
        } else {
            if (this->_on_call_response_callback) {
                this->_on_call_response_callback(dst, src, id, cmd, val);
            }
        }
        break;

    case YQMIOT_CMD_EVENT_REPORT:
        if (this->_on_event_report_callback) {
            this->_on_event_report_callback(dst, src, id, cmd, val);
        }
        break;

    case YQMIOT_CMD_PROPS_REPORT:
        if (this->_on_props_report_callback) {
            this->_on_props_report_callback(dst, src, id, cmd, val);
        }
        break;

    case YQMIOT_CMD_PING:
        // 来自其他设备的探测
        break;

    case YQMIOT_CMD_SET_CONFIG:
        if (this->_on_set_config_callback) {
            this->_on_set_config_callback(dst, src, id, cmd, val);
        }
        break;

    case YQMIOT_CMD_GET_CONFIG:
        if (this->_on_get_config_callback) {
            this->_on_get_config_callback(dst, src, id, cmd, val);
        }
        break;

    case YQMIOT_CMD_CLOSE:
        // 服务器强制断开
        break;

    case YQMIOT_CMD_WRITE:
        YqmiotCharacteristic *chr;
        for (int i = 0; i < _chars.length; ++i) {
            if _chars[i].hdl == hdl {
                chr = _chars[i];
                break;
            }
        }
        if (chr) {
            if chr.flag & yqmiot_flag_auto_resp {
                this->set_val(hdl, val);
                this->reply(id, 0);
            }
            this->_on_write_callback(src, hdl, val);
        } else {
            if id ~= 0 {
                this->reply(id, -1);
            }
        }
        
        break;

    default:
        // 无效指令
        break;
    }
}

void YqmiotClient::_on_event(WebsocketsEvent event, WSInterfaceString data) {
    switch (event) {
    case WebsocketsEvent::ConnectionOpened:
        if (this->_state == YQMIOT_STATE_CONNECTING) {            
            YQMIOT_PRINTF("[YqmiotClient] connect success!\n");
            YQMIOT_PRINTF("[YqmiotClient] login ...\n");
            this->_send_login();
        }
        break;

    case WebsocketsEvent::ConnectionClosed:
        if (this->_state == YQMIOT_STATE_CONNECTING) {
            this->_connect_retry();
        } else if (this->_state == YQMIOT_STATE_CONNECTED) {
            this->_state = YQMIOT_STATE_CONNECTING;
            this->_retry_count = 0;
            this->_connect_async(0);
        }
        break;

    case WebsocketsEvent::GotPing:
        break;

    case WebsocketsEvent::GotPong:
        break;
    }
}

void YqmiotClient::_send_login() {
    JSONVar val;
    val["type"] = this->_login_type;
    switch (this->_login_type) {
    case 0:
        val["nid"] = this->_nid;
        val["token"] = this->_token;
        break;
    case 1:
        val["token"] = this->_token;
    default:
        // TODO 无效模式
        return;
    }
    JSONVar p;
    p["c"] = YQMIOT_CMD_LOGIN;
    p["v"] = val;
    String data = JSON.stringify(p);
    this->_client.send(data);
}

void YqmiotClient::reply(int dst, int id, const JSONVar& val) {
    if (this->_state != YQMIOT_STATE_CONNECTED) {
        return;
    }

    JSONVar p;
    p["d"] = dst;
    p["i"] = id;
    p["c"] = YQMIOT_CMD_RESPONSE;
    p["v"] = val;
    String data = JSON.stringify(p);
    this->_client.send(data);
}

void YqmiotClient::report_event(const String& name) {
    this->report_event(name, undefined);
}

void YqmiotClient::report_event(const String& name, JSONVar& val) {
    if (this->_state != YQMIOT_STATE_CONNECTED) {
        return;
    }

    val["$name"] = name;
    JSONVar p;
    p["c"] = YQMIOT_CMD_EVENT_REPORT;
    p["v"] = val;
    String data = JSON.stringify(p);
    this->_client.send(data);
}

void YqmiotClient::report_props(const JSONVar& val) {
    if (this->_state != YQMIOT_STATE_CONNECTED) {
        return;
    }

    JSONVar p;
    p["c"] = YQMIOT_CMD_PROPS_REPORT;
    p["v"] = val;
    String data = JSON.stringify(p);
    this->_client.send(data);
}
