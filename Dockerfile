FROM ubuntu:16.04

WORKDIR /

ADD . /multichain

RUN apt-get update \
	&& apt-get install -y software-properties-common \
    && apt-get install -y build-essential libtool autotools-dev automake pkg-config libssl-dev git python python-pip bsdmainutils libboost-all-dev\
    && add-apt-repository ppa:bitcoin/bitcoin \
    && apt-get update \
    && apt-get install -y libdb4.8-dev libdb4.8++-dev \
    && pip install pathlib2 \
	&& apt-get -y install nano curl wget

WORKDIR /multichain

RUN echo "==================================== BUILDING V8  ========================================" \
    && export MULTICHAIN_HOME=$(pwd) \
    && mkdir v8build \
    && cd v8build \
    && wget https://github.com/MultiChain/multichain-binaries/raw/master/linux-v8.tar.gz \
    && tar -xvzf linux-v8.tar.gz \
    && echo "================================= BUILDING Multichain ===========================================" \
    && cd $MULTICHAIN_HOME \
    && ./autogen.sh \
	&& ./configure \
	&& make \
	&& strip src/multichaind \
	&& cp src/multichain-util /usr/local/bin/ \
	&& cp src/multichaind /usr/local/bin/ \
	&& cp src/multichain-cli /usr/local/bin/ \

