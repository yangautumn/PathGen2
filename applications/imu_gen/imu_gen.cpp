#include <pathgen/PathGen.h>
#include <Eigen/Geometry>
#include <compass/common/Parameters.h>
#include <compass/common/ParameterReader.h>
#include <glog/logging.h>
#include <gflags/gflags.h>

DEFINE_string(poses, "", "Poses to generate");
DEFINE_string(config_file, "", "Config yaml file with IMU parameters");

uint64_t readPoses(std::string filename,
    std::vector<pathgen::PosePtr>* poses) {
  std::ifstream pose_file(filename, std::ios::in);
  uint64_t ts;
  uint64_t t_0;
  Eigen::Vector4d q;
  Eigen::Vector3d t;

  pose_file >> t_0;
  pose_file.clear();
  pose_file.seekg(0);

  while (pose_file >> ts
      >> q[0] >> q[1] >> q[2] >> q[3]
      >> t[0] >> t[1] >> t[2]) {
    Eigen::Matrix4d T;
    T.topRightCorner(3, 1) = Eigen::Vector3d(t[0], t[1], t[2]);
    T.topLeftCorner(3, 3) = Eigen::Quaterniond(q[0], q[1], q[2], q[3])
      .toRotationMatrix();
    pathgen::PosePtr new_pose(new pathgen::Pose(T));
    poses->push_back(new_pose);
  }
  pose_file.close();
  return ts - t_0;
}

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);
  FLAGS_stderrthreshold = 0;  // INFO: 0, WARNING: 1, ERROR: 2, FATAL: 3
  FLAGS_colorlogtostderr = 1;

  if (FLAGS_config_file.empty() || FLAGS_poses.empty()) {
    LOG(FATAL) << "Need config_file and poses";
  }

  compass::ParameterReader param_reader(FLAGS_config_file);
  compass::CompassParameters params;
  param_reader.GetParameters(params);
  pathgen::ImuParameters imu_params;

  imu_params.a_max = params.imu.a_max;
  imu_params.g_max = params.imu.g_max;
  imu_params.sigma_g_c = params.imu.sigma_g_c;
  imu_params.sigma_bg = params.imu.sigma_bg;
  imu_params.sigma_a_c = params.imu.sigma_a_c;
  imu_params.sigma_ba = params.imu.sigma_ba;
  imu_params.sigma_gw_c = params.imu.sigma_gw_c;
  imu_params.sigma_aw_c = params.imu.sigma_aw_c;
  imu_params.tau = params.imu.tau;
  imu_params.g = params.imu.g;
  imu_params.a0 = params.imu.a0;
  imu_params.rate = params.imu.rate;

  pathgen::ImuMeasurementDeque imu_measurements;
  pathgen::ImuPathOptions path_options;
  path_options.add_noise_to_imu_measurements = true;
  std::vector<pathgen::PosePtr> imu_poses;
  std::vector<pathgen::PosePtr> poses;
  uint64_t duration = readPoses(FLAGS_poses, &poses);
  path_options.duration = 1e-9*duration;

  pathgen::PoseSpline pose_spline =
    pathgen::PathGenerator::GenerateSplineFromPoses(poses);

  pathgen::IMUGenerator::GenerateInertialMeasurements(pose_spline, imu_params,
      path_options, &imu_measurements, &imu_poses);

  pathgen::IMUGenerator::SaveMeasurementFiles(imu_measurements);
  pathgen::SavePoses(imu_poses, "imu_poses.txt");

  return 0;
}