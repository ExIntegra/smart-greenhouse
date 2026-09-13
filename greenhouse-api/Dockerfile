FROM golang:1.27.1-alpine AS builder

WORKDIR /app

COPY go.mod go.sum ./
RUN go mod download

COPY *.go ./

RUN CGO_ENABLED=0 GOOS=linux go build -o /greenhouse-api

FROM alpine:3.23

COPY --from=builder /greenhouse-api /greenhouse-api

EXPOSE 8000

CMD ["/greenhouse-api"]
