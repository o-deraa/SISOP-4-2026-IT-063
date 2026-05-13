FROM ubuntu:latest

# Set working directory
WORKDIR /app

# Install necessary dependencies
RUN apt-get update && apt-get install -y \
    libfuse-dev \
    fuse \
    pkg-config \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Copy server binary dan fuse program
COPY server /app/server
COPY fuse /app/fuse

# Create necessary directories
RUN mkdir -p /app/db \
    && mkdir -p /app/encrypted_storage \
    && mkdir -p /app/fuse_mount

# Make binaries executable
RUN chmod +x /app/server /app/fuse

# Expose port 9000 untuk database service
EXPOSE 9000

# Set environment variable untuk workdir
ENV APP_DB_PATH=/app/db
ENV ENCRYPTED_STORAGE=/app/encrypted_storage
ENV FUSE_MOUNT=/app/fuse_mount

# Start the server
CMD ["./server"]