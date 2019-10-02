FROM ubuntu:latest

RUN apt-get update && apt-get install -y build-essential uuid-dev libz-dev

COPY . /src
WORKDIR /src

RUN make both


FROM ubuntu:latest

COPY --from=0 /src/libmac.so /usr/local/maloader/
COPY --from=0 /src/extract /usr/local/maloader/
COPY --from=0 /src/macho2elf /usr/local/maloader/
COPY --from=0 /src/ld-mac /usr/local/maloader/

ENV PATH="/usr/local/maloader:$PATH"
