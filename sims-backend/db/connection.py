import os

from sqlalchemy import create_engine
from sqlalchemy.orm import create_session, sessionmaker
from sqlalchemy.ext.declarative import declarative_base

# Use POSTGRES_* environment variables from docker-compose
POSTGIS_CONNECT_STR = f"postgresql://{os.getenv('POSTGRES_USER', 'postgres')}:{os.getenv('POSTGRES_PASSWORD', 'postgres')}@"\
    f"{os.getenv('POSTGRES_HOST', 'localhost')}:{os.getenv('POSTGRES_PORT', '5432')}/{os.getenv('POSTGRES_DB', 'sims')}"

# Shared declarative base for all models
Base = declarative_base()

engine = create_engine(POSTGIS_CONNECT_STR)
session = sessionmaker(bind=engine, autocommit=False, autoflush=False)

def get_db():
    db = session()
    try:
        yield db
    finally:
        db.close()