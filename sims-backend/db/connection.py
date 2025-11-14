import os

from sqlalchemy import create_engine
from sqlalchemy.orm import create_session, sessionmaker

POSTGIS_CONNECT_STR = f"postgresql://{os.getenv('DB_USER')}:{os.getenv('DB_PASSWORD')}@"\
    f"{os.getenv('DB_HOST')}:{os.getenv('DB_PORT')}/{os.getenv('DB_NAME')}"


engine = create_engine(POSTGIS_CONNECT_STR)
session = sessionmaker(bind=engine, autocommit=False, autoflush=False)

def get_db():
    db = session()
    try:
        yield db
    finally:
        db.close()