
#if 0

/////////////////////////////////////////////////////////////////////////////////////////

// Utility classes for working with Tesseract parameters.

class ParamsReportWriter {
public:
  ParamsReportWriter(FILE *f)
      : file_(f) 
  {}
  virtual ~ParamsReportWriter() = default;

  virtual void Write(const std::string message) = 0;

protected:
  FILE *file_;
};

class ParamsReportDefaultWriter : public ParamsReportWriter {
public:
  ParamsReportDefaultWriter()
      : ParamsReportWriter(nullptr) 
  {}
  virtual ~ParamsReportDefaultWriter() = default;

  virtual void Write(const std::string message) {
    tprintDebug("{}", message);
  }

protected:
};

class ParamsReportFileDuoWriter : public ParamsReportWriter {
public:
  ParamsReportFileDuoWriter(FILE *f) : ParamsReportWriter(f) {
    is_separate_file_ = (f != nullptr && f != stderr && f != stdout);
  }
  virtual ~ParamsReportFileDuoWriter() = default;

  virtual void Write(const std::string message) {
    // only write via tprintDebug() -- which usually logs to stderr -- when the `f` file destination is an actual file, rather than stderr or stdout.
    // This prevents these report lines showing up in duplicate on the console.
    if (is_separate_file_) {
      tprintDebug("{}", message);
    }
    size_t len = message.length();
    if (fwrite(message.c_str(), 1, len, file_) != len) {
      tprintError("Failed to write params-report line to file. {}\n", strerror(errno));
    }
  }

protected:
  bool is_separate_file_;
};

class ParamsReportStringWriter : public ParamsReportWriter {
public:
  ParamsReportStringWriter() : ParamsReportWriter(nullptr) {}
  virtual ~ParamsReportStringWriter() = default;

  virtual void Write(const std::string message) {
    buffer += message;
  }

  std::string to_string() const {
    return buffer;
  }

protected:
  std::string buffer;
};



  // Thismethod ALWAYS reports via `tprintf()` at least; when `f` is a valid, non-stdio
  static void ReportParamsUsageStatistics(FILE *fp, const ParamsVectors *member_params, const char *section_title);

  static void ReportParamsUsageStatistics(ParamsReportWriter &w, const ParamsVectors *member_params, int section_level, const char *section_title);


#endif


