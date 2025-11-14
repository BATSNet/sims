import logging
import os
import shutil
from typing import Tuple
from datetime import date, time, datetime

from dotenv import load_dotenv
from fastapi import APIRouter, UploadFile, Depends
from magic.compat import mime_magic
from pydantic import UUID4
from pydantic.v1 import BaseModel
from sqlalchemy.orm import Session, declarative_base
from ..models.contact_model import RawContactModel, ContactEntry
from ..db.connection import get_db

load_dotenv()

# local storage (for now)
OBJECT_STORAGE_DIR = os.environ["OBJECT_STORAGE_DIR"]

contact_router = APIRouter()

logger = logging.getLogger(__name__)


@contact_router.post("/create")
def create_contact(
        contact: RawContactModel,
        db: Session = Depends(get_db)
):
    '''
        Receive raw data from the user and enhance it
        to write a complete Contact entry into the database.

        1. create uuid
        2. process content (image, video, audio, text)
            - summarize with LLM
        3. categorize
        4. add tags

    '''

    uid = UUID4()
    phone = contact.user_phone
    logger.info(f"Processing contact of user phone {phone}.")

    content = contact.media_file

    # process content
    content_path = None
    mime_type = None
    text_description = None
    audio_transcript = None
    transcript_summary = None
    # either text description or content must be provided
    assert (contact.text_description is not None) or (content is not None)

    if contact.text_description is not None:
        logger.info("Content type: text description")

    if content is not None:
        mime_type = content.content_type
        logger.info(f"Content type: {mime_type}")
        mediatype, filetype = content.content_type.split("/")

        obj_filename = f"{phone}_{str(uid)[:4]}.{filetype}"
        obj_filepath = os.path.join(OBJECT_STORAGE_DIR, obj_filename)
        with open(obj_filepath, "wb") as buffer:
            shutil.copyfileobj(content.file, buffer)
        logger.info(f"{mediatype.capitalize()} stored as {obj_filepath}")
        content_path = obj_filepath

        # create transcription and summarization
        if mime_type.startswith("audio"):
            # TODO
            audio_transcript = None
            transcript_summary = None

    entry = ContactEntry(
        id=uid,
        user_phone=contact.user_phone,
        location=contact.location, # TODO lon,lat -> geojson
        date=contact.date,
        time=contact.time,

        content_path=content_path,
        mime_type=mime_type,
        text_description=text_description,
        audio_transcript=audio_transcript,
        transcript_summary=transcript_summary,

        status='reported',

        metadata=contact.metadata,

        created_at=datetime.now(),
        updated_at=datetime.now()

    )
    db.add(entry)
    db.commit()
    logger.info("Entry written to database.")