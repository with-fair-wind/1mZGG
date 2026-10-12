#include <QCoreApplication>
#include <exception>

#include <gtest/gtest.h>

#include "dss/core/diagnostics/emergency_report.h"

/// @brief Keep the Qt application alive until all network tests and workers are destroyed.
/// @param argc Number of command-line arguments.
/// @param argv Command-line argument storage, valid for the application lifetime.
/// @return GoogleTest result, or failure if application setup or teardown throws.
int main(int argc, char** argv) try {
    QCoreApplication application(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} catch (const std::exception& error) {
    Dss::Core::reportEmergencyFailure(error.what());
    Dss::Core::reportEmergencyFailure("\n");
    return 1;
} catch (...) {
    Dss::Core::reportEmergencyFailure("DSS: Qt test application failed\n");
    return 1;
}
