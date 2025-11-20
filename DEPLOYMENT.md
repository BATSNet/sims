# SIMS Deployment Guide

This guide explains how to deploy the SIMS application to your Hetzner server.

## Prerequisites

1. SSH access to your Hetzner server (91.99.179.35)
2. SSH key-based authentication set up
3. Docker and Docker Compose installed on the remote server
4. rsync installed on your local machine

## Initial Setup

### 1. Set Up SSH Access

First, ensure you can SSH into your server:

```bash
ssh root@91.99.179.35
```

If you haven't set up SSH keys yet:

```bash
# Generate SSH key (if you don't have one)
ssh-keygen -t rsa -b 4096

# Copy your SSH key to the server
ssh-copy-id root@91.99.179.35
```

### 2. Install Docker on Server

If Docker is not installed on your Hetzner server:

```bash
ssh root@91.99.179.35

# Install Docker
curl -fsSL https://get.docker.com -o get-docker.sh
sh get-docker.sh

# Install Docker Compose
apt-get update
apt-get install -y docker-compose-plugin

# Verify installation
docker --version
docker compose version
```

### 3. Create Deployment Configuration

Copy the example configuration file:

```bash
cp deploy.config.example deploy.config
```

Edit `deploy.config` with your settings:

```bash
REMOTE_HOST=91.99.179.35
REMOTE_USER=root
REMOTE_PATH=/opt/sims
LOCAL_ENV_FILE=.env.production
```

### 4. Create Production Environment File

Create `.env.production` with your production settings:

```bash
# Database
POSTGRES_USER=sims_user
POSTGRES_PASSWORD=your_secure_password_here
POSTGRES_DB=sims

# API Keys
FEATHERLESS_API_KEY=your_featherless_api_key
DEEPINFRA_API_KEY=your_deepinfra_api_key

# Environment
ENVIRONMENT=production
```

## Deployment

### Quick Deploy

Run the deployment script:

```bash
bash deploy.sh
```

This script will:
1. Test SSH connection
2. Create necessary directories on the server
3. Sync all required files (excluding development files)
4. Copy the .env file
5. Build and restart Docker containers

### What Gets Deployed

The deployment includes:
- `sims-backend/` - Backend Python code
- `docker/` - Dockerfile for backend
- `postgis/` - Database initialization scripts
- `resources/` - Resource files
- `docker-compose.yml` - Base Docker configuration
- `docker-compose.prod.yml` - Production overrides
- `.env` - Environment variables (from .env.production)

The deployment excludes:
- `sims-app/` - Flutter app (not needed on server)
- `venv/`, `__pycache__/` - Python virtual environments
- `.git/` - Git repository
- `uploads/` - User uploads (managed on server)
- Development files and logs

## Production Configuration

The `docker-compose.prod.yml` file provides production-specific settings:

- Containers restart automatically unless stopped
- Ports are bound to localhost only (127.0.0.1)
- Caddy or another reverse proxy should handle external access
- Source code is not mounted (uses code baked into Docker image)

## Setting Up Caddy (Reverse Proxy)

Since you mentioned setting up Caddy, here's a basic configuration:

### 1. Install Caddy on Server

```bash
ssh root@91.99.179.35

# Install Caddy
apt install -y debian-keyring debian-archive-keyring apt-transport-https
curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/gpg.key' | gpg --dearmor -o /usr/share/keyrings/caddy-stable-archive-keyring.gpg
curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/debian.deb.txt' | tee /etc/apt/sources.list.d/caddy-stable.list
apt update
apt install caddy
```

### 2. Configure Caddy

Edit `/etc/caddy/Caddyfile`:

```
your-domain.com {
    reverse_proxy localhost:8000

    encode gzip

    log {
        output file /var/log/caddy/sims.log
    }
}
```

Reload Caddy:

```bash
systemctl reload caddy
```

## Manual Deployment Steps

If you need to deploy manually:

```bash
# 1. SSH into server
ssh root@91.99.179.35

# 2. Navigate to deployment directory
cd /opt/sims

# 3. Pull latest changes (if using git on server)
git pull

# 4. Rebuild and restart containers
docker-compose -f docker-compose.yml -f docker-compose.prod.yml down
docker-compose -f docker-compose.yml -f docker-compose.prod.yml build
docker-compose -f docker-compose.yml -f docker-compose.prod.yml up -d

# 5. Check logs
docker-compose logs -f backend
```

## Monitoring and Maintenance

### View Logs

```bash
# View all logs
ssh root@91.99.179.35 'cd /opt/sims && docker-compose logs -f'

# View only backend logs
ssh root@91.99.179.35 'cd /opt/sims && docker-compose logs -f backend'

# View only database logs
ssh root@91.99.179.35 'cd /opt/sims && docker-compose logs -f db'
```

### Check Container Status

```bash
ssh root@91.99.179.35 'cd /opt/sims && docker-compose ps'
```

### Restart Services

```bash
# Restart all services
ssh root@91.99.179.35 'cd /opt/sims && docker-compose -f docker-compose.yml -f docker-compose.prod.yml restart'

# Restart only backend
ssh root@91.99.179.35 'cd /opt/sims && docker-compose restart backend'
```

### Database Backup

```bash
# Create backup
ssh root@91.99.179.35 'cd /opt/sims && docker-compose exec -T db pg_dump -U sims_user sims > backup_$(date +%Y%m%d_%H%M%S).sql'

# Download backup to local machine
scp root@91.99.179.35:/opt/sims/backup_*.sql ./backups/
```

## Troubleshooting

### Deployment Script Fails

1. Check SSH connection:
   ```bash
   ssh root@91.99.179.35 'echo "Connection OK"'
   ```

2. Verify deploy.config exists and has correct values

3. Ensure rsync is installed locally:
   ```bash
   rsync --version
   ```

### Containers Won't Start

1. Check Docker logs:
   ```bash
   ssh root@91.99.179.35 'cd /opt/sims && docker-compose logs'
   ```

2. Verify .env file exists on server:
   ```bash
   ssh root@91.99.179.35 'cat /opt/sims/.env'
   ```

3. Check disk space:
   ```bash
   ssh root@91.99.179.35 'df -h'
   ```

### Database Connection Issues

1. Check database container is running:
   ```bash
   ssh root@91.99.179.35 'cd /opt/sims && docker-compose ps db'
   ```

2. Test database connection:
   ```bash
   ssh root@91.99.179.35 'cd /opt/sims && docker-compose exec db psql -U sims_user -d sims -c "SELECT 1;"'
   ```

## Security Considerations

1. Never commit `deploy.config` or `.env.production` to git
2. Use strong passwords in .env.production
3. Keep API keys secure
4. Regularly update Docker images
5. Set up firewall rules (UFW):
   ```bash
   ufw allow 22/tcp    # SSH
   ufw allow 80/tcp    # HTTP
   ufw allow 443/tcp   # HTTPS
   ufw enable
   ```
6. Consider setting up automated backups
7. Enable automatic security updates on the server

## Updating After Initial Deployment

To deploy updates after the initial setup:

```bash
# Simply run the deploy script again
bash deploy.sh
```

The script will:
- Sync only changed files
- Rebuild containers if Dockerfile or code changed
- Restart containers with zero downtime for database

## Rollback

If a deployment fails, you can rollback:

```bash
ssh root@91.99.179.35

cd /opt/sims

# Stop current containers
docker-compose -f docker-compose.yml -f docker-compose.prod.yml down

# Restore from backup if needed
# git checkout <previous-commit>  # if using git on server

# Restart with previous version
docker-compose -f docker-compose.yml -f docker-compose.prod.yml up -d
```
