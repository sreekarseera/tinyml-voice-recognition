// New portable MFCC baseline. Used identically by desktop training and device.
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
namespace voice {
constexpr int Rate=16000, Window=16000, Frame=320, Hop=320, FFT=512, Bands=26, Coeffs=13, Features=26;
constexpr double Pi=3.14159265358979323846;
inline void fft(std::array<std::complex<float>,FFT>& x) {
  for(unsigned i=1,j=0;i<FFT;++i){
    unsigned bit=FFT>>1;
    for(;j&bit;bit>>=1) j^=bit;
    j^=bit;if(i<j)std::swap(x[i],x[j]);
  }
  for(int len=2;len<=FFT;len<<=1){
    const std::complex<float> step(std::cos(-2*Pi/len),std::sin(-2*Pi/len));
    for(int i=0;i<FFT;i+=len){
      std::complex<float> w(1,0);
      for(int j=0;j<len/2;++j){auto a=x[i+j],b=x[i+j+len/2]*w;x[i+j]=a+b;x[i+j+len/2]=a-b;w*=step;}
    }
  }
}
inline std::array<float,Features> extract(const float* audio) {
  std::array<float,Features> out{};
  std::array<double,Coeffs> sums{},squares{};
  std::array<float,Bands+2> edges{};
  const double low=2595*std::log10(1+40.0/700), high=2595*std::log10(1+7600.0/700);
  for(int b=0;b<Bands+2;++b)edges[b]=700*(std::pow(10,(low+(high-low)*b/(Bands+1))/2595)-1);
  int frames=0;
  for(int start=0;start+Frame<=Window;start+=Hop){
    std::array<std::complex<float>,FFT> spectrum{};
    for(int n=0;n<Frame;++n)spectrum[n]=audio[start+n]*float(.5-.5*std::cos(2*Pi*n/(Frame-1)));
    fft(spectrum);
    std::array<float,Bands> logs{};
    for(int b=0;b<Bands;++b){
      double energy=0;
      for(int k=0;k<=FFT/2;++k){
        float hz=float(k)*Rate/FFT;
        float w=hz<edges[b+1]?(hz-edges[b])/(edges[b+1]-edges[b]):(edges[b+2]-hz)/(edges[b+2]-edges[b+1]);
        if(w>0)energy+=w*std::norm(spectrum[k])/FFT;
      }
      logs[b]=std::log(std::max(energy,1e-12));
    }
    for(int c=0;c<Coeffs;++c){
      double value=0;
      for(int b=0;b<Bands;++b)value+=logs[b]*std::cos(Pi*c*(b+.5)/Bands);
      value*=std::sqrt((c==0?1.0:2.0)/Bands);
      sums[c]+=value;squares[c]+=value*value;
    }
    ++frames;
  }
  for(int c=0;c<Coeffs;++c){
    const double mean=sums[c]/frames;
    out[c]=mean;out[c+Coeffs]=std::sqrt(std::max(0.0,squares[c]/frames-mean*mean));
  }
  return out;
}
} // namespace voice
