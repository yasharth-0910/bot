from pydantic import BaseModel
from typing import List

class BoundingBox(BaseModel):
    x: float
    y: float
    width: float
    height: float
    confidence: float

class ImageDimensions(BaseModel):
    width: int
    height: int

class Detection(BaseModel):
    bbox: BoundingBox
    confidence: float

class DetectionResponse(BaseModel):
    total_people: int
    detections: List[Detection]
    image_dimensions: ImageDimensions
    processing_time: float 