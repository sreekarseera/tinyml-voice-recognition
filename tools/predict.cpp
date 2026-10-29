#include "classifier.h"
#include <iostream>
#include <iomanip>
int main(){
  std::array<float,voice::Features> f{};
  for(auto &v:f)if(!(std::cin>>v)||!std::isfinite(v))return 2;
  auto values=voice::scores(f);
  for(size_t i=0;i<values.size();++i)std::cout<<(i?",":"")<<std::setprecision(9)<<values[i];
  std::cout<<"\n";
}
