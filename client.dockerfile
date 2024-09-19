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

RUN mkdir -p /root/client/proto /root/client/src /root/client/build

COPY client/src /root/client/src
COPY client/CMakeLists.txt /root/client/
COPY proto /root/client/proto
COPY client/libs/httplib.h /root/client/build

RUN cd /root/client/build && \
cmake .. && \
cmake --build .

CMD /root/client/build/client


