FROM python:3.11-slim

# Install system dependencies for building C++
RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the engine source
COPY engine/ ./engine/

# Build the C++ engine
RUN cd engine && \
    cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j$(nproc)

# Copy backend requirements and install Python dependencies
COPY backend/requirements.txt ./backend/
RUN pip install --no-cache-dir -r backend/requirements.txt

# Copy backend source
COPY backend/ ./backend/

# Set environment variables for the FastAPI app to find the engine and output
ENV QR_ENGINE_BIN=/app/engine/build/bin/qr_engine
ENV QR_OUTPUT_ROOT=/app/output
# Ensure python finds the app module
ENV PYTHONPATH=/app/backend

# Create output directories so the API has a place to write files
RUN mkdir -p /app/output/output/holder /app/output/output/tools /app/output/.webcache

# Run from the backend directory
WORKDIR /app/backend

EXPOSE 8000
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
