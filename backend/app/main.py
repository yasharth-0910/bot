from fastapi import FastAPI, UploadFile, File
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from loguru import logger
import time
from datetime import datetime

from .model import YOLODetector
from .schemas import DetectionResponse

app = FastAPI(title="Person Detection API")

# Configure CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173"],  # React app
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Initialize the YOLO model
model = YOLODetector()

@app.get("/")
async def root():
    return {"message": "Person Detection API"}

@app.post("/detect/", response_model=DetectionResponse)
async def detect_people(file: UploadFile = File(...)):
    start_time = time.time()
    
    try:
        # Process the image and get detections
        detections = await model.process_image(file)
        
        # Log the detection
        logger.info(
            f"Detection request at {datetime.now().isoformat()}: "
            f"Found {detections.total_people} people with "
            f"confidence scores: {[d.confidence for d in detections.detections]}"
        )
        
        return detections
        
    except Exception as e:
        logger.error(f"Error processing image: {str(e)}")
        return JSONResponse(
            status_code=500,
            content={"error": str(e)}
        )
    finally:
        process_time = time.time() - start_time
        logger.debug(f"Processing time: {process_time:.2f} seconds") 