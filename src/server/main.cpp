#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "voice_service_impl.h"

int main(int argc, char** argv) {
    std::string server_address("0.0.0.0:50051");
    voice::VoiceServiceImpl service;

    grpc::ServerBuilder builder;
    // 设置服务器地址和端口
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // 注册服务
    builder.RegisterService(&service);
    
    // 构建并启动服务器
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;

    // 等待服务器终止
    server->Wait();

    return 0;
} 