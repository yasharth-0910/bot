import torch
import cv2
import numpy as np
from fastapi import UploadFile
import io
from .schemas import DetectionResponse, Detection, BoundingBox, ImageDimensions
import time
from ultralytics import YOLO
import os

class YOLODetector:
    def __init__(self):
        # Load YOLOv5n model using Ultralytics package
        self.model = YOLO('./models/yolov5nu.pt')
        # Set confidence threshold
        self.model.conf = 0.25
        # Only detect people (class 0)
        self.model.classes = [0]

    async def process_image(self, file: UploadFile) -> DetectionResponse:
        start_time = time.time()
        
        # Read image file
        contents = await file.read()
        nparr = np.frombuffer(contents, np.uint8)
        img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        
        if img is None:
            raise ValueError("Could not decode image")
        
        # Get image dimensions
        height, width = img.shape[:2]
        
        # Run inference
        results = self.model(img)
        
        # Process detections
        detections = []
        for result in results:
            for box in result.boxes:
                if box.cls == 0:  # class 0 is person
                    x1, y1, x2, y2 = box.xyxy[0].tolist()
                    conf = box.conf.item()
                    
                    # Convert to x, y, width, height format
                    bbox = BoundingBox(
                        x=x1,
                        y=y1,
                        width=x2 - x1,
                        height=y2 - y1,
                        confidence=conf
                    )
                    
                    detections.append(Detection(
                        bbox=bbox,
                        confidence=conf
                    ))
        
        return DetectionResponse(
            total_people=len(detections),
            detections=detections,
            image_dimensions=ImageDimensions(width=width, height=height),
            processing_time=time.time() - start_time
        ) 