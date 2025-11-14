from typing import Optional, List, Dict, Any, Tuple

from fastapi import UploadFile
from pydantic import BaseModel, Field, constr
from datetime import date, datetime
from datetime import date, time


class RawContactModel(BaseModel):
    user_phone: int
    location: Tuple[float, float]
    date: date
    time: time

    media_file: Optional[UploadFile] = None

    text_description: Optional[str] = None

    category: Optional[str]

    metadata: dict


class ContactEntry(BaseModel):
    id: Optional[int] = None  # BIGSERIAL primary key (usually auto-generated)
    user_phone: int  # Regex for phone number with optional '+'
    location: Dict[str, Any]  # GeoJSON Point (latitude, longitude), matching PostGIS GEOMETRY(POINT, 4326)

    date: Optional[date] = Field(default_factory=date.today)
    time: Optional[time] = Field(default_factory=datetime.utcnow)

    content_path: Optional[str]
    mime_type: Optional[str]

    text_description: Optional[str]

    audio_transcript: Optional[str]
    transcript_summary: Optional[str]

    status: constr(regex=r'^(reported|escalated|deescalated)$')

    category: str = 'Unclassified'

    tags: List[str] = Field(default_factory=list)

    metadata: Dict[str, Any] = Field(default_factory=dict)

    created_at: Optional[datetime] = Field(default_factory=datetime.utcnow)
    updated_at: Optional[datetime] = Field(default_factory=datetime.utcnow)

    class Config:
        orm_mode = True
