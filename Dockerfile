FROM fedora:latest

RUN yum update -y && yum install -y \
boost-devel \
cmake \
clang \
git \
z3 \
&& dnf install boost-static

WORKDIR /solidity

COPY . .

# Build
RUN mkdir build \
&& cd build \
&& cmake .. \
&& make

CMD ["echo", "Use a command"]
