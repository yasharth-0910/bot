export interface BoundingBox {
  x: number;
  y: number;
  width: number;
  height: number;
  confidence: number;
}

export interface Detection {
  bbox: BoundingBox;
  confidence: number;
}

export interface ImageDimensions {
  width: number;
  height: number;
}

export interface DetectionResponse {
  total_people: number;
  detections: Detection[];
  image_dimensions: ImageDimensions;
  processing_time: number;
} 