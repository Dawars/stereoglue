// Copyright (C) 2024 ETH Zurich.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//
//     * Neither the name of ETH Zurich nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Please contact the author of this library if you have any questions.
// Author: Daniel Barath (majti89@gmail.com)
#pragma once

#include "scoring/types.h"
#include "samplers/types.h"
#include "local_optimization/types.h"
#include "termination/types.h"

namespace stereoglue {

    struct LocalOptimizationSettings
    {
        size_t maxIterations = 50,
            sampleSizeMultiplier = 7;
    };

    struct RANSACSettings 
    {
        size_t minIterations = 1000, // Minimum number of iterations
            maxIterations = 5000; // Maximum number of iterations
        size_t coreNumber = 4; // Number of cores
        double inlierThreshold = 1.5; // Inlier threshold
        double confidence = 0.99; // Confidence
        
        scoring::ScoringType scoring = 
            scoring::ScoringType::MAGSAC; // Scoring type

        samplers::SamplerType sampler = 
            samplers::SamplerType::Uniform; // Sampler type
            
        local_optimization::LocalOptimizationType localOptimization = 
            local_optimization::LocalOptimizationType::NestedRANSAC; // Local optimization type

        local_optimization::LocalOptimizationType finalOptimization = 
            local_optimization::LocalOptimizationType::IRLS; // Final optimization type

        termination::TerminationType terminationCriterion = 
            termination::TerminationType::RANSAC; // Termination criterion type

        LocalOptimizationSettings localOptimizationSettings,
            finalOptimizationSettings;
    };

}