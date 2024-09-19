FROM ubuntu:latest

RUN apt-get update && apt-get install -y cmake build-essential git protobuf-compiler


WORKDIR /root

RUN git clone --recurse-submodules -b v1.62.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc
RUN cd grpc && \
mkdir -p cmake/build && \
cd cmake/build && \
cmake -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR ../.. && \
make -j 4 && \
make install

RUN mkdir -p /root/server/proto /root/server/src /root/server/build

COPY server/src /root/server/src
COPY server/CMakeLists.txt /root/server/
COPY proto /root/server/proto

RUN cd /root/server/build && \
cmake .. && \
cmake --build .

CMD /root/server/build/server


