FROM gcc:latest

WORKDIR /app

COPY . .

RUN make

EXPOSE 8000

CMD ["./bin/server"]
