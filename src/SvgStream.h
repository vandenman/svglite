#ifndef __SVG_STREAM__
#define __SVG_STREAM__

#include <fstream>
#include <sstream>
#include <Rcpp.h>
#include "utils.h"

namespace svglite { namespace internal {

template <typename T>
void write_double(T& stream, double data) {
  std::streamsize prec = stream.precision();
  uint8_t newprec = std::fabs(data) >= 1 || data == 0. ? prec : std::ceil(-std::log10(std::fabs(data))) + 1;
  stream << std::setprecision(newprec) << data << std::setprecision(prec);
}

}} // namespace svglite::internal


class SvgStream {
  public:

  virtual ~SvgStream() {};

  virtual void write(int data) = 0;
  virtual void write(double data) = 0;
  virtual void write(const char* data) = 0;
  virtual void write(const std::string& data) = 0;
  virtual void write(char data) = 0;

  void put(char data) {
    write(data);
  }

  virtual void flush() = 0;
  virtual void finish() = 0;
};

template <typename T>
SvgStream& operator<<(SvgStream& object, const T& data) {
  object.write(data);
  return object;
}
template <>
SvgStream& operator<<(SvgStream& object, const double& data) {
  // Make sure negative zeros are converted to positive zero for
  // reproducibility of SVGs
  object.write(dbl_format(data));
  return object;
}

class SvgStreamFile : public SvgStream {
  std::ofstream stream_;

public:
  SvgStreamFile(const std::string& path) {
    stream_.open(R_ExpandFileName(path.c_str()));

    if (stream_.fail())
      Rcpp::stop("cannot open stream " + path);

    stream_ << std::fixed << std::setprecision(2);
  }

  SvgStreamFile(const std::string& path, int pageno) {

    char buf[PATH_MAX+1];
    snprintf(buf, PATH_MAX, path.c_str(), pageno);
    buf[PATH_MAX] = '\0';

    stream_.open(R_ExpandFileName(buf));
    if (stream_.fail())
      Rcpp::stop("cannot open stream " + std::string(buf));

    stream_ << std::fixed << std::setprecision(2);
  }

  void write(int data)            { stream_ << data; }
  void write(double data)         { svglite::internal::write_double(stream_, data); }
  void write(const char* data)    { stream_ << data; }
  void write(char data)           { stream_ << data; }
  void write(const std::string& data) { stream_ << data; }

  // Adding a final newline here creates problems on Windows when
  // seeking back to original position. So we only write the newline
  // in finish()
  void flush() {
    stream_ << "</svg>";
    stream_.seekp(-6, std::ios_base::cur);
    stream_.flush();
  }

  void finish() {
    stream_ << "</svg>\n";
    stream_.flush();
  }

  ~SvgStreamFile() {
    stream_.close();
  }
};


class SvgStreamString : public SvgStream {
  std::stringstream stream_;
  Rcpp::Environment env_;

public:
  SvgStreamString(Rcpp::Environment env): env_(env) {
    stream_ << std::fixed << std::setprecision(2);
    env_["is_closed"] = false;
  }

  void write(int data)                { stream_ << data; }
  void write(double data)             { svglite::internal::write_double(stream_, data); }
  void write(const char* data)        { stream_ << data; }
  void write(char data)               { stream_ << data; }
  void write(const std::string& data) { stream_ << data; }

  void flush() {
  }

  void finish() {
    // When device is closed, stream_ will be destroyed, so we can no longer
    // get the svg string from stream_. In this case, we save the final string
    // to the environment env, so that R can read from env$svg_string even
    // after device is closed.
    env_["is_closed"] = true;

    stream_.flush();
    std::string svgstr = stream_.str();
    // If the current svg is empty, we also make the string empty
    // Otherwise append "</svg>" to make it a valid SVG
    if(!svgstr.empty()) {
      svgstr.append("</svg>");
    }
    if (env_.exists("svg_string")) {
      Rcpp::CharacterVector str = env_["svg_string"];
      str.push_back(svgstr);
      env_["svg_string"] = str;
    } else {
      env_["svg_string"] = svgstr;
    }

    // clear the stream
    stream_.str(std::string());
    stream_.clear();
  }

  Rcpp::XPtr<std::stringstream> string_src() {
    // `false` means this pointer should not be "deleted" by R
    // The object will be automatically destroyed when device is closed
    return Rcpp::XPtr<std::stringstream>(&stream_, false);
  }
};


#endif
