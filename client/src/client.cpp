#include "proto/messenger.pb.h"
#include <cstdlib>
#include <google/protobuf/json/json.h>
#include <grpc++/grpc++.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <list>
#include <memory>
#include <mutex>
#include <proto/messenger.grpc.pb.h>
#include <string>
#include <thread>
#include <httplib.h>
#include <google/protobuf/util/json_util.h>

class MessageBuffer {
public:
    void AddMessage(::mes_grpc::ReadResponse& message) {
        std::string mes_str;
        auto a = google::protobuf::json::MessageToJsonString(message, &mes_str);

        std::unique_lock<std::mutex> guard(mutex_);
        ++cnt_;
        messages_.push_back(mes_str);
    }

    std::string GetAndFlush() {
        std::string res = "[";

        std::unique_lock<std::mutex> guard(mutex_);
        auto it = messages_.begin();
        res.reserve(it->size() * cnt_);
        while (it != messages_.end()) {
            --cnt_;
            res += *it;
            if (cnt_ != 0) {
                res += ",";
            }
            messages_.erase(it++);
        }

        res += "]";

        return res;
    }

private:
    std::mutex mutex_;
    std::list<std::string> messages_;
    size_t cnt_ = 0;
};

class MessengerGrpcClient {
public:
    MessengerGrpcClient(std::shared_ptr<::grpc::Channel> channel) : stub_(::mes_grpc::MessengerServer::NewStub(channel)), 
    messages_(std::make_unique<MessageBuffer>()) {
    }

    ~MessengerGrpcClient() {
        listener.join();
    }

    void StartListen() {
        listener = std::thread(&MessengerGrpcClient::StartReading, this);
    }
    
    std::string SendMessage(std::string_view author, std::string_view text) {
        ::mes_grpc::SendRequest request;
        request.set_author(author);
        request.set_text(text);

        ::grpc::ClientContext context;
        ::mes_grpc::SendResponse response;
        stub_->SendMessage(&context, request, &response);

        std::string out;
        auto res = google::protobuf::json::MessageToJsonString(response, &out);

        return out;
    }

    std::string GetMessagesAndFlush() {
        return messages_->GetAndFlush();
    }

private:
    void StartReading() {
        ::mes_grpc::ReadRequest request;
        ::grpc::ClientContext context;

        auto reader = stub_->ReadMessages(&context, request);

        ::mes_grpc::ReadResponse message;
        while (reader->Read(&message)) {
            messages_->AddMessage(message);
        }
    }

    std::unique_ptr<::mes_grpc::MessengerServer::Stub> stub_;
    std::unique_ptr<MessageBuffer> messages_;

    std::mutex mutex_;
    std::thread listener;
};

class HttpServer {
public:
    HttpServer() 
    : grpc_(::grpc::CreateChannel(std::string(std::getenv("MESSENGER_SERVER_ADDR")), ::grpc::InsecureChannelCredentials())) {

        http_.Post("/sendMessage", [&](const httplib::Request& req, httplib::Response& res) {
            ::mes_grpc::SendRequest send_req;
            auto result = google::protobuf::json::JsonStringToMessage(req.body, &send_req);
            std::string grpc_response = grpc_.SendMessage(send_req.author(), send_req.text());
            res.set_content(grpc_response, "text/plain");
        });

        http_.Post("/getAndFlushMessages", [&](const httplib::Request&, httplib::Response& res) {
            std::string grpc_response = grpc_.GetMessagesAndFlush();
            res.set_content(grpc_response, "text/plain");
        });

    }

    void StartServer() {
        grpc_.StartListen();
        http_.listen("0.0.0.0", std::stoi(std::getenv("MESSENGER_HTTP_PORT")));
    }

private:
    httplib::Server http_;
    MessengerGrpcClient grpc_;
};



int main() {
    HttpServer server;
    server.StartServer();
}