#pragma once
#include "features.h"
#include <trained_model.h>
namespace voice {
inline std::array<float,Classes> scores(const std::array<float,Features>& features) {
  std::array<float,Classes> result{};
  for(int c=0;c<Classes;++c){
    result[c]=Bias[c];
    for(int j=0;j<Features;++j)
      result[c]+=Weights[c][j]*WeightScale*(features[j]-FeatureMean[j])*FeatureInvStd[j];
  }
  return result;
}
}
