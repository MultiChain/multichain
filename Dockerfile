FROM ubuntu:16.04

WORKDIR /

ADD . /multichain

RUN apt-get update \
	&& apt-get -y install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils \
	&& apt-get -y install libboost-all-dev \
	&& apt-get -y install git \
	&& apt-get -y install software-properties-common \
	&& add-apt-repository ppa:bitcoin/bitcoin \
	&& apt-get update \
	&& apt-get -y install libdb4.8-dev libdb4.8++-dev \
	&& apt-get -y install nano \
	&& apt-get -y install curl

WORKDIR /multichain

RUN ./autogen.sh \
	&& ./configure \
	&& make \
	&& cp src/multichain-util /usr/local/bin/ \
	&& cp src/multichaind /usr/local/bin/ \
	&& cp src/multichain-cli /usr/local/bin/

