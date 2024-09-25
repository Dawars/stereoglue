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
//     * Neither the name of Czech Technical University nor the
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
// Author: Daniel Barath (barath.daniel@sztaki.mta.hu)
#pragma once

#include <vector>
#include <Eigen/Core>
#include "abstract_local_optimizer.h"
#include "../samplers/uniform_random_sampler.h"
#include "../utils/types.h"

namespace stereoglue
{
	namespace local_optimization
	{
		// Templated class for estimating a model for RANSAC. This class is purely a
		// virtual class and should be implemented for the specific task that RANSAC is
		// being used for. Two methods must be implemented: estimateModel and residual. All
		// other methods are optional, but will likely enhance the quality of the RANSAC
		// output.
		class NestedRANSACOptimizer : public LocalOptimizer
		{
		protected:
			size_t maxIterations;
			size_t sampleSizeMultiplier;

		public:
			NestedRANSACOptimizer() : maxIterations(50), 
				sampleSizeMultiplier(7)
			{

			}

			~NestedRANSACOptimizer() {}

			// Set the maximum number of iterations
			void setMaxIterations(const size_t maxIterations_)
			{
				maxIterations = maxIterations_;
			}

			// Set the sample size multiplier
			void setSampleSizeMultiplier(const size_t sampleSizeMultiplier_)
			{
				sampleSizeMultiplier = sampleSizeMultiplier_;
			}

			// The function for estimating the model parameters from the data points with multimatches.
			void run(
				const DataMatrix &kDataSrc_,
				const DataMatrix &kDataDst_,
				const DataMatrix &kMatches_,
				const DataMatrix &kMatchScores_,
				const std::vector<std::pair<size_t, size_t>> &kInliers_,
				const models::Model &kModel_,
				const scoring::Score &kScore_,
				const estimator::Estimator *kEstimator_,
				const scoring::AbstractScoring *kScoring_,
				models::Model &estimatedModel_,
				scoring::Score &estimatedScore_,
				std::vector<std::pair<size_t, size_t>> &estimatedInliers_) const
			{
				// The invalid score
				static const scoring::Score kInvalidScore;

				// The sampler used for selecting minimal samples
				samplers::UniformRandomSampler sampler;

				// Initialize the estimated model and score
				estimatedModel_ = kModel_;

				// The size of the non-minimal samples
				const size_t kNonMinimalSampleSize = sampleSizeMultiplier * kEstimator_->nonMinimalSampleSize();
				size_t currentSampleSize;

				// The currently estimated models
				std::vector<models::Model> currentlyEstimatedModels;
				scoring::Score currentScore = kInvalidScore;
				std::vector<std::pair<size_t, size_t>> currentInliers;
				currentInliers.reserve(kDataSrc_.rows());

				// Allocate memory for the current sample
				std::vector<size_t> currentSample(kNonMinimalSampleSize);
				std::vector<std::pair<size_t, size_t>> currentMatches(kNonMinimalSampleSize);

				// Calculate the score of the estimated model
				estimatedScore_ = kScoring_->score(
					kDataSrc_,
					kDataDst_,
					kMatches_,
					kMatchScores_, 
					estimatedModel_, 
					kEstimator_, 
					estimatedInliers_);
				sampler.initialize(estimatedInliers_.size() - 1);

				// The inner RANSAC loop
				for (size_t iteration = 0; iteration < maxIterations; ++iteration)
				{
					// Calculate the current sample size
					currentSampleSize = estimatedInliers_.size() - 1;
					if (currentSampleSize >= kNonMinimalSampleSize)
						currentSampleSize = kNonMinimalSampleSize;

					// Break if the sample size is too small
					if (currentSampleSize < kEstimator_->nonMinimalSampleSize())
						break;

					// Check if the sample size is equal to the number of inliers
					if (currentSampleSize == estimatedInliers_.size())
					{
						// Copy the inliers to the current sample
						for (size_t sampleIdx = 0; sampleIdx < currentSampleSize; ++sampleIdx)
							currentMatches[sampleIdx] = estimatedInliers_[sampleIdx];
					} else
					{
						// Sample minimal set
						if (!sampler.sample(estimatedInliers_.size(), // Data matrix
							currentSampleSize, // Selected minimal sample 
							&currentSample[0])) // Sample indices
							continue;

						// Replace the sample indices with the data point indices
						for (size_t sampleIdx = 0; sampleIdx < currentSampleSize; ++sampleIdx)
							currentMatches[sampleIdx] = estimatedInliers_[currentSample[sampleIdx]];
					}

					// Remove the previous models
					currentlyEstimatedModels.clear();
					currentlyEstimatedModels.emplace_back(estimatedModel_);

					// The correspondences for the estimation based on the inliers
					Eigen::MatrixXd correspondences(currentSampleSize, 4);
					for (size_t i = 0; i < currentSampleSize; ++i)
					{
						const auto &match = currentMatches[i];
						correspondences.row(i) <<
							kDataSrc_(match.first, 0),
							kDataSrc_(match.first, 1),
							kDataDst_(match.second, 0),
							kDataDst_(match.second, 1);
					}

					if (!kEstimator_->estimateModelNonminimal(
						correspondences,  // The data points
						nullptr, 
						correspondences.rows(),
						&currentlyEstimatedModels,
						nullptr))
						return;

					// Calculate the scoring of the estimated model
					for (const auto &model : currentlyEstimatedModels)
					{
						// Calculate the score of the estimated model
						currentInliers.clear();
						currentScore = kScoring_->score(
							kDataSrc_,
							kDataDst_,
							kMatches_,
							kMatchScores_, 
							model, 
							kEstimator_, 
							currentInliers);

						// Check if the current model is better than the previous one
						if (currentScore > estimatedScore_)
						{
							// Update the estimated model
							estimatedModel_ = model;
							estimatedScore_ = currentScore;
							currentInliers.swap(estimatedInliers_);
							sampler.initialize(estimatedInliers_.size() - 1);
						}
					}
				}
			}
			
			// The function for estimating the model parameters from the data points.
			void run(const DataMatrix &kData_, // The data points
				const std::vector<size_t> &kInliers_, // The inliers of the previously estimated model
				const models::Model &kModel_, // The previously estimated model 
				const scoring::Score &kScore_, // The of the previously estimated model
				const estimator::Estimator *kEstimator_, // The estimator used for the model estimation
				const scoring::AbstractScoring *kScoring_, // The scoring object used for the model estimation
				models::Model &estimatedModel_, // The estimated model
				scoring::Score &estimatedScore_, // The score of the estimated model
				std::vector<size_t> &estimatedInliers_) const // The inliers of the estimated model
			{
				// The invalid score
				static const scoring::Score kInvalidScore = scoring::Score();

				// The sampler used for selecting minimal samples
				samplers::UniformRandomSampler sampler;

				// Initialize the estimated model and score
				estimatedModel_ = kModel_;

				// The size of the non-minimal samples
				const size_t kNonMinimalSampleSize = sampleSizeMultiplier * kEstimator_->sampleSize();
				size_t currentSampleSize;

				// The currently estimated models
				std::vector<models::Model> currentlyEstimatedModels;
				scoring::Score currentScore = kInvalidScore;
				std::vector<size_t> currentInliers;
				currentInliers.reserve(kData_.rows());

				// Allocate memory for the current sample
				size_t *currentSample = new size_t[kNonMinimalSampleSize];

				// Calculate the score of the estimated model
				estimatedScore_ = kScoring_->score(kData_, estimatedModel_, kEstimator_, estimatedInliers_);
				sampler.initialize(estimatedInliers_.size() - 1);

				// The inner RANSAC loop
				for (size_t iteration = 0; iteration < maxIterations; ++iteration)
				{
					// Calculate the current sample size
					currentSampleSize = estimatedInliers_.size() - 1;
					if (currentSampleSize >= kNonMinimalSampleSize)
						currentSampleSize = kNonMinimalSampleSize;

					// Break if the sample size is too small
					if (currentSampleSize < kEstimator_->sampleSize())
						break;

					// Check if the sample size is equal to the number of inliers
					if (currentSampleSize == estimatedInliers_.size())
					{
						// Copy the inliers to the current sample
						for (size_t sampleIdx = 0; sampleIdx < currentSampleSize; ++sampleIdx)
							currentSample[sampleIdx] = estimatedInliers_[sampleIdx];
					} else
					{
						// Sample minimal set
						if (!sampler.sample(estimatedInliers_.size(), // Data matrix
							currentSampleSize, // Selected minimal sample 
							currentSample)) // Sample indices
							continue;

						// Replace the sample indices with the data point indices
						for (size_t sampleIdx = 0; sampleIdx < currentSampleSize; ++sampleIdx)
							currentSample[sampleIdx] = estimatedInliers_[currentSample[sampleIdx]];
					}

					// Remove the previous models
					currentlyEstimatedModels.clear();

					// Estimate the model
					if (!kEstimator_->estimateModelNonminimal(
						kData_,  // The data points
						currentSample, // Selected minimal sample
						currentSampleSize, // The size of the minimal sample
						&currentlyEstimatedModels, // The estimated models
						nullptr)) // The indices of the inliers
						continue;

					// Calculate the scoring of the estimated model
					for (const auto &model : currentlyEstimatedModels)
					{
						// Calculate the score of the estimated model
						currentInliers.clear();
						currentScore = kScoring_->score(kData_, model, kEstimator_, currentInliers);

						// Check if the current model is better than the previous one
						if (currentScore > estimatedScore_)
						{
							// Update the estimated model
							estimatedModel_ = model;
							estimatedScore_ = currentScore;
							currentInliers.swap(estimatedInliers_);
							sampler.initialize(estimatedInliers_.size() - 1);
						}
					}
				}

				// Clean up
				delete[] currentSample;
			}

		};
	}
}  // namespace gcransac