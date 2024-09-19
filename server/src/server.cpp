#include <condition_variable>
#include <google/protobuf/timestamp.pb.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/status.h>
#include <list>
#include <memory>
#include <mutex>
#include <proto/messenger.grpc.pb.h>
#include <proto/messenger.pb.h>
#include <unistd.h>
#include <cstdlib>

class CloseWaiter {
public:
    void Lock() {
        std::unique_lock<std::mutex> guard(mutex_);
        while (stream_open_) {
            is_alive_.wait(guard);
        }
    }

    void Unlock() {
        std::unique_lock<std::mutex> guard(mutex_);
        stream_open_ = false;
        is_alive_.notify_one();
    }

private:
    bool stream_open_ = true;
    std::condition_variable is_alive_;;
    std::mutex mutex_;
};

class Stream {
public:
    explicit Stream(::grpc::ServerWriter<::mes_grpc::ReadResponse>* stream, ::grpc::ServerContext* context) 
    : stream_(stream), context_(context), close_waiter_(std::make_unique<CloseWaiter>()) {};

    ::grpc::ServerWriter<::mes_grpc::ReadResponse>* GetStream() {
        return stream_;
    }

    bool IsAlive() {
        return !context_->IsCancelled();
    }

    void Wait() {
        close_waiter_->Lock();
    }

    void WaitRelease() {
        close_waiter_->Unlock();
    }

private:
    ::grpc::ServerContext* context_;
    ::grpc::ServerWriter<::mes_grpc::ReadResponse>* stream_;
    std::unique_ptr<CloseWaiter> close_waiter_;
};

class MessageStreamBuffer {
public:
    Stream* Subscribe(::grpc::ServerContext* context, ::grpc::ServerWriter<::mes_grpc::ReadResponse>* stream) {
        std::unique_ptr<Stream> stream_ptr = std::make_unique<Stream>(stream, context);
        Stream* return_ptr = stream_ptr.get();

        std::unique_lock<std::mutex> guard(mutex_);
        streams_.push_back(std::move(stream_ptr));

        return return_ptr;
    }

    void SendMessage(::mes_grpc::SendRequest message, std::unique_ptr<::google::protobuf::Timestamp> time) {
        ::mes_grpc::ReadResponse responseMessage;
        responseMessage.set_author(message.author());
        responseMessage.set_text(message.text());
        responseMessage.set_allocated_sendtime(time.release());

        std::unique_lock<std::mutex> guard(mutex_);
        auto it = streams_.begin();
        while (it != streams_.end()) {
            auto& stream = *it;
            if (!stream->IsAlive()) {
                stream->WaitRelease();
                streams_.erase(it++);
                continue;
            }

            stream->GetStream()->Write(responseMessage);
            ++it;
        }
    }

private:
    std::list<std::unique_ptr<Stream>> streams_;
    std::mutex mutex_;
};

class MessengerImpl : public ::mes_grpc::MessengerServer::Service {
public:
    ~MessengerImpl() override = default;


    ::grpc::Status SendMessage(::grpc::ServerContext* context, const ::mes_grpc::SendRequest* request, ::mes_grpc::SendResponse* response) override {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        std::unique_ptr<google::protobuf::Timestamp> sendTime = std::make_unique<google::protobuf::Timestamp>();
        sendTime->set_seconds(tv.tv_sec);
        sendTime->set_nanos(tv.tv_usec * 1000);
        response->set_allocated_sendtime(sendTime.release());
        
        std::unique_ptr<google::protobuf::Timestamp> sendTimeRelease = std::make_unique<google::protobuf::Timestamp>(response->sendtime());
        messageStreamBuffer_.SendMessage(*request, std::move(sendTimeRelease));
        return grpc::Status::OK;
    }

    ::grpc::Status ReadMessages(::grpc::ServerContext* context, const ::mes_grpc::ReadRequest* request, ::grpc::ServerWriter< ::mes_grpc::ReadResponse>* writer) override {
    
        Stream* stream = messageStreamBuffer_.Subscribe(context, writer);
        stream->Wait();

        return grpc::Status::OK;
    }

private: 
    MessageStreamBuffer messageStreamBuffer_;
};

int main() {
    MessengerImpl service;
    grpc::ServerBuilder builder;

    std::string address = "0.0.0.0:";
    address += std::string(std::getenv("MESSENGER_SERVER_PORT"));

    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    server->Wait();

    return 0;
}