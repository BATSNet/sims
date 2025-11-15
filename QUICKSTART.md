# Quick Start: Deploy SIMS to Production

This guide gets you up and running on your Hetzner server in minutes.

## Server Info
- IP: 91.99.179.35
- User: root

## Step 1: Configure Deployment

```bash
# Copy deployment config
cp deploy.config.example deploy.config

# Edit deploy.config (should already have correct IP)
# Just verify REMOTE_HOST=91.99.179.35
```

## Step 2: Set Up Environment Variables

```bash
# Copy production environment template
cp .env.production.example .env.production

# Edit .env.production and set:
# - POSTGRES_PASSWORD (use a strong password)
# - FEATHERLESS_API_KEY
# - DEEPINFRA_API_KEY
# - CADDY_DOMAIN (use 91.99.179.35 or your domain if you have one)
# - CADDY_EMAIL (for SSL certificate notifications)
```

## Step 3: Deploy

```bash
bash deploy.sh
```

This will:
- Copy all files to /opt/sims on your server
- Build Docker images
- Start PostgreSQL, Backend, and Caddy
- Expose your API on ports 80 (HTTP) and 443 (HTTPS)

## Step 4: Access Your API

Your API is now accessible at:

**Using IP:**
- `http://91.99.179.35/` (Caddy will handle this)
- `https://91.99.179.35/` (if SSL is configured)

**Using Domain (if configured):**
- `http://yourdomain.com/`
- `https://yourdomain.com/` (auto SSL via Let's Encrypt)

## Step 5: Update Your Flutter App

Edit `sims-app/lib/src/config/app_config.dart`:

```dart
static const String prodBaseUrl = 'http://91.99.179.35';
// Or if using domain:
// static const String prodBaseUrl = 'https://yourdomain.com';

static const bool isDevelopment = false;  // Switch to production
```

## Quick Commands

```bash
# View logs
bash remote-cmd.sh logs

# Check status
bash remote-cmd.sh status

# Restart services
bash remote-cmd.sh restart

# SSH to server
bash remote-cmd.sh ssh
```

## API Endpoints

Once deployed, test with:

```bash
# Health check
curl http://91.99.179.35/health

# Or with your domain
curl http://yourdomain.com/health
```

## Using a Custom Domain

If you have a domain (e.g., api.yourdomain.com):

1. Point your domain A record to: 91.99.179.35

2. Update .env.production:
   ```bash
   CADDY_DOMAIN=api.yourdomain.com
   CADDY_EMAIL=your-email@example.com
   ```

3. Redeploy:
   ```bash
   bash deploy.sh
   ```

Caddy will automatically get a Let's Encrypt SSL certificate for HTTPS!

## Troubleshooting

### Can't access API
```bash
# Check if containers are running
bash remote-cmd.sh status

# Check Caddy logs
bash remote-cmd.sh logs caddy

# Check backend logs
bash remote-cmd.sh logs backend
```

### Firewall issues
```bash
bash remote-cmd.sh ssh

# On server, check firewall
ufw status

# Allow HTTP/HTTPS if needed
ufw allow 80/tcp
ufw allow 443/tcp
```

### SSL not working
- Make sure your domain points to 91.99.179.35
- Check CADDY_EMAIL is set correctly in .env.production
- Give it a few minutes for Let's Encrypt to issue certificate
- Check Caddy logs: `bash remote-cmd.sh logs caddy`

## Next Steps

1. Test your API endpoints
2. Point your Flutter app to the production URL
3. Set up regular backups: `bash remote-cmd.sh backup-db`
4. Monitor logs for issues

For detailed documentation, see DEPLOYMENT.md
