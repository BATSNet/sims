#!/bin/bash

# SIMS Deployment Script for Hetzner Server
# This script deploys the Docker containers to the remote Hetzner server

set -e

# Load deployment configuration
if [ ! -f "deploy.config" ]; then
    echo "Error: deploy.config file not found!"
    echo "Please copy deploy.config.example to deploy.config and configure it."
    exit 1
fi

source deploy.config

# Validate required variables
if [ -z "$REMOTE_HOST" ] || [ -z "$REMOTE_USER" ] || [ -z "$REMOTE_PATH" ]; then
    echo "Error: Missing required configuration in deploy.config"
    echo "Required: REMOTE_HOST, REMOTE_USER, REMOTE_PATH"
    exit 1
fi

echo "========================================="
echo "SIMS Deployment to Hetzner Server"
echo "========================================="
echo "Remote Host: $REMOTE_HOST"
echo "Remote User: $REMOTE_USER"
echo "Remote Path: $REMOTE_PATH"
echo "========================================="

# Test SSH connection
echo "Testing SSH connection..."
if ! ssh -o ConnectTimeout=5 "$REMOTE_USER@$REMOTE_HOST" "echo 'SSH connection successful'"; then
    echo "Error: Cannot connect to remote server via SSH"
    echo "Please ensure:"
    echo "  1. SSH key is set up correctly"
    echo "  2. Server is accessible"
    echo "  3. REMOTE_HOST and REMOTE_USER are correct in deploy.config"
    exit 1
fi

# Create remote directory if it doesn't exist
echo "Creating remote directory structure..."
ssh "$REMOTE_USER@$REMOTE_HOST" "mkdir -p $REMOTE_PATH/{sims-backend,docker,postgis,resources}"

# Sync files to remote server (excluding unnecessary files)
echo "Syncing files to remote server..."
rsync -avz --progress \
    --exclude 'venv/' \
    --exclude '__pycache__/' \
    --exclude '*.pyc' \
    --exclude '.git/' \
    --exclude 'sims-app/' \
    --exclude 'uploads/' \
    --exclude 'research/' \
    --exclude '.env' \
    --exclude 'deploy.config' \
    --exclude '*.log' \
    --exclude '.idea/' \
    --exclude '.vscode/' \
    --exclude 'test_*.py' \
    ./ "$REMOTE_USER@$REMOTE_HOST:$REMOTE_PATH/"

# Copy .env file if specified in config
if [ ! -z "$LOCAL_ENV_FILE" ] && [ -f "$LOCAL_ENV_FILE" ]; then
    echo "Copying environment file..."
    scp "$LOCAL_ENV_FILE" "$REMOTE_USER@$REMOTE_HOST:$REMOTE_PATH/.env"
else
    echo "Warning: No .env file specified or found. Make sure .env exists on remote server."
fi

# Deploy to remote server
echo "Deploying on remote server..."
ssh "$REMOTE_USER@$REMOTE_HOST" "cd $REMOTE_PATH && bash -s" << 'ENDSSH'
    echo "Pulling latest base images..."
    docker-compose pull db

    echo "Building backend image..."
    docker-compose build backend

    echo "Stopping existing containers..."
    docker-compose -f docker-compose.yml -f docker-compose.prod.yml down

    echo "Starting containers with production configuration..."
    docker-compose -f docker-compose.yml -f docker-compose.prod.yml up -d

    echo "Waiting for services to start..."
    sleep 5

    echo "Checking container status..."
    docker-compose ps

    echo "Checking backend logs..."
    docker-compose logs --tail=20 backend
ENDSSH

echo "========================================="
echo "Deployment completed successfully!"
echo "========================================="
echo "To view logs, run:"
echo "  ssh $REMOTE_USER@$REMOTE_HOST 'cd $REMOTE_PATH && docker-compose logs -f'"
echo ""
echo "To check status, run:"
echo "  ssh $REMOTE_USER@$REMOTE_HOST 'cd $REMOTE_PATH && docker-compose ps'"
