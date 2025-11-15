#!/bin/bash

# SIMS Remote Command Helper
# Quick access to common remote server operations

set -e

if [ ! -f "deploy.config" ]; then
    echo "Error: deploy.config file not found!"
    exit 1
fi

source deploy.config

COMPOSE_CMD="cd $REMOTE_PATH && docker-compose -f docker-compose.yml -f docker-compose.prod.yml"

case "$1" in
    logs)
        echo "Fetching logs from remote server..."
        ssh "$REMOTE_USER@$REMOTE_HOST" "$COMPOSE_CMD logs -f ${2:-backend}"
        ;;
    status)
        echo "Checking container status..."
        ssh "$REMOTE_USER@$REMOTE_HOST" "$COMPOSE_CMD ps"
        ;;
    restart)
        echo "Restarting services..."
        ssh "$REMOTE_USER@$REMOTE_HOST" "$COMPOSE_CMD restart ${2:-}"
        ;;
    stop)
        echo "Stopping services..."
        ssh "$REMOTE_USER@$REMOTE_HOST" "$COMPOSE_CMD down"
        ;;
    start)
        echo "Starting services..."
        ssh "$REMOTE_USER@$REMOTE_HOST" "$COMPOSE_CMD up -d"
        ;;
    ssh)
        echo "Connecting to remote server..."
        ssh "$REMOTE_USER@$REMOTE_HOST"
        ;;
    backup-db)
        echo "Creating database backup..."
        BACKUP_FILE="backup_$(date +%Y%m%d_%H%M%S).sql"
        ssh "$REMOTE_USER@$REMOTE_HOST" "cd $REMOTE_PATH && docker-compose exec -T db pg_dump -U \${POSTGRES_USER:-postgres} \${POSTGRES_DB:-sims} > $BACKUP_FILE"
        echo "Backup created: $BACKUP_FILE"
        echo "Downloading backup..."
        scp "$REMOTE_USER@$REMOTE_HOST:$REMOTE_PATH/$BACKUP_FILE" "./backups/"
        echo "Backup downloaded to ./backups/$BACKUP_FILE"
        ;;
    shell)
        echo "Opening shell in backend container..."
        ssh -t "$REMOTE_USER@$REMOTE_HOST" "cd $REMOTE_PATH && docker-compose exec backend /bin/bash"
        ;;
    *)
        echo "SIMS Remote Command Helper"
        echo ""
        echo "Usage: bash remote-cmd.sh <command> [options]"
        echo ""
        echo "Commands:"
        echo "  logs [service]      View logs (default: backend)"
        echo "  status              Check container status"
        echo "  restart [service]   Restart service(s)"
        echo "  stop                Stop all services"
        echo "  start               Start all services"
        echo "  ssh                 SSH into the server"
        echo "  backup-db           Backup database and download"
        echo "  shell               Open shell in backend container"
        echo ""
        echo "Examples:"
        echo "  bash remote-cmd.sh logs"
        echo "  bash remote-cmd.sh logs db"
        echo "  bash remote-cmd.sh restart backend"
        echo "  bash remote-cmd.sh status"
        exit 1
        ;;
esac
