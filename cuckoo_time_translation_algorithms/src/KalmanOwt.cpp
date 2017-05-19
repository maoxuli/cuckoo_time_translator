#include <cuckoo_time_translator/KalmanOwt.h>

#include <iostream>
#include <stdexcept>

#include <console_bridge/console.h>

namespace cuckoo_time_translator {

KalmanOwt::KalmanOwt() :
  R_ (0),
  isInitialized_(false),
  lastUpdateDeviceTime_(0),
  dt_ (0)
{
}

KalmanOwt::~KalmanOwt() {
}

LocalTime KalmanOwt::translateToLocalTimestamp(const RemoteTime remoteTimeTics) const {
  if(!isInitialized_) {
    throw std::runtime_error("The filter not initialized yet!");
  }
  const double dt = remoteTimeTics - lastUpdateDeviceTime_;
  return LocalTime(remoteTimeTics + x_(0) + dt * x_(1));
}

void KalmanOwt::printNameAndConfig(std::ostream & out) const {
  out << "Kalman("
      << "updateRate=" << config.updateRate
      << ", "
      << "sigmaSkew=" << config.sigmaSkew
      << ", "
      << "sigmaOffset=" << config.sigmaOffset
      << ", "
      << "sigmaInitSkew=" << config.sigmaInitSkew
      << ", "
      << "sigmaInitOffset=" << config.sigmaInitOffset
      << ", "
      << "outlierThreshold=" << config.outlierThreshold
      << ")";
}
void KalmanOwt::printState(std::ostream & out) const {
  out << "offset=" << x_(0) << ", skew=" << x_(1) << ", dt=" << dt_;
}

void KalmanOwt::reset() {
  isInitialized_ = 0;
}

LocalTime KalmanOwt::updateAndTranslateToLocalTimestamp(const RemoteTime remoteTimeTics, const LocalTime localTimeSecs) {
  if(!isInitialized_) {
    initialize(remoteTimeTics, localTimeSecs);
    return localTimeSecs;
  }

  const double dt = remoteTimeTics - lastUpdateDeviceTime_;

  if(dt >= config.updateRate){
    dt_=dt;
    Eigen::Matrix2d F;
    F << 1, dt_, 0, 1;

    // Prediction
    x_ = F * x_;
    P_ = F * P_ * F.transpose() + dt_ * Q_;
    lastUpdateDeviceTime_ = remoteTimeTics;

    // Update
    const double S = H_ * P_ * H_.transpose() + R_;

    Eigen::Vector2d K;
    K = P_ * H_.transpose() * (1 / S);

    const double measurementResidual = localTimeSecs - remoteTimeTics - H_ * x_;

    const double mahalDistance = sqrt(measurementResidual*measurementResidual*(1.0/S));

    if(mahalDistance > config.outlierThreshold){
      logWarn("KalmanOwt: local_time=%g, remote_time=%g -> measurement_residual=%g, mahal_distance=%g!", localTimeSecs, remoteTimeTics, measurementResidual, mahalDistance);
    } else {
      x_ = x_ + K * measurementResidual;
      P_ = (Eigen::Matrix2d::Identity() - K * H_) * P_;
    }
  }
  return translateToLocalTimestamp(remoteTimeTics);
}

bool KalmanOwt::isReadyToTranslate() const {
  return isInitialized_;
}

void KalmanOwt::initialize(const double device_time, const double localTimeSecs) {
  x_.setZero();
  x_[0] = localTimeSecs - device_time;

  P_.setZero();
  P_(0,0) = config.sigmaInitOffset * config.sigmaInitOffset;
  P_(1,1) = config.sigmaInitSkew * config.sigmaInitSkew;

  Q_.setZero();
  Q_(1,1) = config.sigmaSkew * config.sigmaSkew;

  R_ = config.sigmaOffset * config.sigmaOffset;

  H_.setZero();
  H_(0,0) = 1;

  lastUpdateDeviceTime_ = device_time;
  isInitialized_ = true;
}

KalmanOwt* KalmanOwt::cloneImpl() const {
  return new KalmanOwt(*this);
}

}
