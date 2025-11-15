import logging
import os
from apscheduler import AsyncScheduler, ConflictPolicy
from apscheduler.triggers.cron import CronTrigger
from apscheduler.triggers.interval import IntervalTrigger
from keycloak import KeycloakOpenID
from starlette.responses import  Response
from starlette.types import ASGIApp, Scope, Receive, Send

from assets.util import store_app_location_data
from content.login import authenticate_user, async_authenticate_user
from fastapi import Request, HTTPException, FastAPI, WebSocket
from fastapi.responses import RedirectResponse
from nicegui import Client, app
from eda.heartbeat import organize_news_feed, validate_licenses, cleanup_old_images, cleanup_old_files, \
    refresh_ms_tokens

from dotenv import load_dotenv
from starlette.middleware.base import BaseHTTPMiddleware

from database.pg_vector import SecurePostgresCache

from models.chat_model import ChatSessionORM
from db.connection import get_db

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

load_dotenv(".env")

unrestricted_page_routes = ['/health',
                            '/login',
                            '/logout',
                            '/register',
                            '/register/plans',
                            '/register/account',
                            '/register/complete',
                            '/onboarding',
                            '/koauth2callback',
                            '/oauth2callback',
                            '/ms-oauth2callback',
                            '/login-success']

db = get_db()

keycloak_openid = KeycloakOpenID(
    server_url=os.getenv("KEYCLOAK_SERVER_URL", "http://localhost:8081/"),
    client_id=os.getenv("KEYCLOAK_CLIENT_ID", "aivory"),
    realm_name="aivory",
    client_secret_key=os.getenv("KEYCLOAK_CLIENT_SECRET", "aivory-client-secret")
)

def summarize_chats(db):
    # checks all chats summarizes
    # those that have changes since last summarization
    # compares last_summarized with last_modified.
    # for the selection of sessions where summaries are not up to date,
    # get all the text messages and audio transcripts:
    # go through the chatmessages and for the audio fetch the transcripts;
    # for text get the text directly
    # then do sequential api calls to llm inference endpoints to summarize
    # then write the summaries into 'summary' field
    # and updates the 'last_summarized'

    selection = db.query(ChatSessionORM).filter(
                    ChatSessionORM.last_summarized < ChatSessionORM.last_modified)




class SchedulerMiddleware:
    def __init__(
            self,
            app: ASGIApp,
            scheduler: AsyncScheduler,
    ) -> None:
        self.app = app
        self.scheduler = scheduler

    async def __call__(self, scope: Scope, receive: Receive, send: Send) -> None:
        try:
            if scope["type"] == "lifespan":
                async with self.scheduler:
                    await self.scheduler.cleanup()
                    try:
                        for schedule in await self.scheduler.get_schedules():
                            await self.scheduler.remove_schedule(schedule.id)
                    except Exception as e:
                        logger.error(f"Error in remove_schedule")

                    try:

                        await self.scheduler.add_schedule(
                            summarize_chats,
                            trigger=CronTrigger(minute=1, timezone="UTC"),
                            id="summarize_chats",
                            job_executor='threadpool',
                            conflict_policy=ConflictPolicy.replace
                        )
                        #await refresh_ms_tokens() #running once on startup
                    except Exception as e:
                        logger.error(f"Error in ms_token_refresh: {e}", exc_info=True)


                    await self.scheduler.start_in_background()
                    await self.app(scope, receive, send)
            else:
                await self.app(scope, receive, send)
        except Exception as e:
            logger.error(f"Scheduler middleware error: {e}",stack_info=True, exc_info=True)
