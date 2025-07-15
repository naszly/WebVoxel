FROM emscripten/emsdk:4.0.10 AS builder

WORKDIR /app
RUN apt-get update && apt-get install -y git python3-pip
RUN pip install --upgrade cmake
COPY client/ client/
COPY webgpu_dawn/ webgpu_dawn/
COPY CMakeLists.txt CMakeLists.txt
COPY publish.sh publish.sh
COPY server.py server.py
RUN chmod +x publish.sh
RUN ./publish.sh

FROM python:3-alpine AS webserver
WORKDIR /app
COPY --from=builder /app/publish /app/publish
COPY --from=builder /app/server.py /app/server.py
EXPOSE 8000
CMD ["python3", "server.py", "publish", "0.0.0.0"]