## Akış Diyagramı (Mermaid)

```mermaid
flowchart LR
  CAM[CameraController] --> YOLO[YoloDetector (OpenCV DNN)]
  YOLO --> IFF[IFF HSV ROI]
  IFF --> TRK[TargetTracker (IoU + EMA)]
  TRK --> SEG[BalloonSegmentor (HSV+Contour)]
  SEG --> DST[DistanceEstimator]
  DST --> BAL[BallisticsManager (LUT+offset)]
  BAL --> AIM[AimSolver]
  AIM --> SM[CombatStateMachine + Geofence]
  SM --> TRG[TriggerController]
  TRG --> TURRET[TurretController]
  AIM --> UDP[UdpVideoStreamer]
  AIM --> TCP[TcpTelemetryServer]
```
