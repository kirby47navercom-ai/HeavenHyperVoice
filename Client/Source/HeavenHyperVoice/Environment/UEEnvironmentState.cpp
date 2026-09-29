#include "UEEnvironmentState.h"
void BlendEnvironment(FUEEnvironmentState& current,const FUEEnvironmentState& target,double alpha) {
    current.Enabled=target.Enabled;
    current.DayFraction=FMath::Fmod(current.DayFraction+FMath::FindDeltaAngleDegrees(current.DayFraction*360.0,target.DayFraction*360.0)/360.0*alpha+1.0,1.0);
    current.YearFraction=FMath::Fmod(current.YearFraction+FMath::FindDeltaAngleDegrees(current.YearFraction*360.0,target.YearFraction*360.0)/360.0*alpha+1.0,1.0);
    current.SunElevationDegrees=FMath::Lerp(current.SunElevationDegrees,target.SunElevationDegrees,alpha);
    current.SunAzimuthDegrees=FMath::Fmod(current.SunAzimuthDegrees+FMath::FindDeltaAngleDegrees(current.SunAzimuthDegrees*1.0,target.SunAzimuthDegrees*1.0)/1.0*alpha+360.0,360.0);
    current.TideLevelM=FMath::Lerp(current.TideLevelM,target.TideLevelM,alpha);
    current.WaveHeightM=FMath::Lerp(current.WaveHeightM,target.WaveHeightM,alpha);
    current.SandstormIntensity=FMath::Lerp(current.SandstormIntensity,target.SandstormIntensity,alpha);
    current.GroundTemperatureC=FMath::Lerp(current.GroundTemperatureC,target.GroundTemperatureC,alpha);
    current.SurfaceWaterMm=FMath::Lerp(current.SurfaceWaterMm,target.SurfaceWaterMm,alpha);
    current.IceMm=FMath::Lerp(current.IceMm,target.IceMm,alpha);
    current.SoilMoisture=FMath::Lerp(current.SoilMoisture,target.SoilMoisture,alpha);
    current.TimeScale=FMath::Lerp(current.TimeScale,target.TimeScale,alpha);
}
