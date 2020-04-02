//
// Created by mb4416 on 24/03/2020.
//

#include "Job.h"
PassSegmentJob::PassSegmentJob(TaskResult &&result) : result(std::move(result)) {}
