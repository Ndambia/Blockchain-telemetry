import os
from dotenv import load_dotenv

# Load .env variables into the environment
load_dotenv()

class Config:
    # Pusher configuration
    PUSHER_APP_ID = os.getenv('PUSHER_APP_ID', 'your_app_id')
    PUSHER_KEY = os.getenv('PUSHER_KEY', 'your_key')
    PUSHER_SECRET = os.getenv('PUSHER_SECRET', 'your_secret')
    PUSHER_CLUSTER = os.getenv('PUSHER_CLUSTER', 'mt1')

    # Blockchain settings (custom)
    BLOCK_SIZE_LIMIT = 1024  # bytes
    TRANSACTION_LIMIT = 50
    BLOCK_TIME_TARGET = 30  # seconds
