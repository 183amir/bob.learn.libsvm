/**
 * @file machine/cxx/SVM.cc
 * @date Sat Dec 17 14:41:56 2011 +0100
 * @author Andre Anjos <andre.anjos@idiap.ch>
 *
 * @brief Implementation of the SVM machine using libsvm
 *
 * Copyright (C) 2011-2013 Idiap Research Institute, Martigny, Switzerland
 */

#include <string>
#include <cmath>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <bob/machine/SVM.h>
#include <bob/core/check.h>
#include <bob/core/logging.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <algorithm>

static bool is_colon(char i) { return i == ':'; }

bob::machine::SVMFile::SVMFile (const std::string& filename):
  m_filename(filename),
  m_file(m_filename.c_str()),
  m_shape(0),
  m_n_samples(0)
{
  if (!m_file) {
    boost::format s("cannot open file '%s'");
    s % filename;
    throw std::runtime_error(s.str());
  }

  //scan the whole file, gets the shape and total size
  while (m_file.good()) {
    //gets the next non-empty line
    std::string line;
    while (!line.size()) {
      if (!m_file.good()) break;
      std::getline(m_file, line);
      boost::trim(line);
    }

    if (!m_file.good()) break;

    int label;
    size_t pos;
    char separator;
    double value;
    size_t n_values = std::count_if(line.begin(), line.end(), is_colon);

    std::istringstream iss(line);
    iss >> label;

    for (size_t k=0; k<n_values; ++k) {
      iss >> pos >> separator >> value;
      if (m_shape < pos) m_shape = pos;
    }

    ++m_n_samples;
  }

  //reset the file to then begin to read it properly
  m_file.clear();
  m_file.seekg(0, std::ios_base::beg);
}

bob::machine::SVMFile::~SVMFile() {
}

void bob::machine::SVMFile::reset() {
  m_file.close();
  m_file.open(m_filename.c_str());
}

bool bob::machine::SVMFile::read(int& label, blitz::Array<double,1>& values) {
  if ((size_t)values.extent(0) != m_shape) {
    boost::format s("file '%s' contains %d entries per sample, but you gave me an array with only %d positions");
    s % m_filename % m_shape % values.extent(0);
    throw std::runtime_error(s.str());
  }

  //read the data.
  return read_(label, values);
}

bool bob::machine::SVMFile::read_(int& label, blitz::Array<double,1>& values) {

  //if the file is at the end, just raise, you should have checked
  if (!m_file.good()) return false;

  //gets the next non-empty line
  std::string line;
  while (!line.size()) {
    if (!m_file.good()) return false;
    std::getline(m_file, line);
    boost::trim(line);
  }

  std::istringstream iss(line);
  iss >> label;

  int pos;
  char separator;
  double value;

  values = 0; ///zero values all over as the data is sparse on the files

  for (size_t k=0; k<m_shape; ++k) {
    iss >> pos >> separator >> value;
    values(pos-1) = value;
  }

  return true;
}

/**
 * A wrapper, to standardize this function.
 */
static void svm_model_free(svm_model*& m) {
#if LIBSVM_VERSION >= 300
  svm_free_and_destroy_model(&m);
#else
  svm_destroy_model(m);
#endif
}

blitz::Array<uint8_t,1> bob::machine::svm_pickle
(const boost::shared_ptr<svm_model> model)
{
  std::string tmp_filename = bob::core::tmpfile(".svm");

  //save it to a temporary file
  if (svm_save_model(tmp_filename.c_str(), model.get())) {
    boost::format s("cannot save SVM to file `%s' while copying model");
    s % tmp_filename;
    throw std::runtime_error(s.str());
  }

  //gets total size of file
  struct stat filestatus;
  stat(tmp_filename.c_str(), &filestatus);

  //reload the data from the file in binary format
  std::ifstream binfile(tmp_filename.c_str(), std::ios::binary);
  blitz::Array<uint8_t,1> buffer(filestatus.st_size);
  binfile.read(reinterpret_cast<char*>(buffer.data()), filestatus.st_size);

  //unlink the temporary file
  boost::filesystem::remove(tmp_filename);

  //finally, return the pickled data
  return buffer;
}

static boost::shared_ptr<svm_model> make_model(const char* filename) {
  boost::shared_ptr<svm_model> retval(svm_load_model(filename),
      std::ptr_fun(svm_model_free));
#if LIBSVM_VERSION > 315
  if (retval) retval->sv_indices = 0; ///< force initialization: see ticket #109
#endif
  return retval;
}

/**
 * Reverts the pickling process, returns the model
 */
boost::shared_ptr<svm_model> bob::machine::svm_unpickle
(const blitz::Array<uint8_t,1>& buffer) {

  std::string tmp_filename = bob::core::tmpfile(".svm");

  std::ofstream binfile(tmp_filename.c_str(), std::ios::binary);
  binfile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
  binfile.close();

  //reload the file using the appropriate libsvm loading method
  boost::shared_ptr<svm_model> retval = make_model(tmp_filename.c_str());

  if (!retval) {
    boost::format s("cannot open model file '%s'");
    s % tmp_filename;
    throw std::runtime_error(s.str());
  }

  //unlinks the temporary file
  boost::filesystem::remove(tmp_filename);

  //finally, return the pickled data
  return retval;
}

void bob::machine::SupportVector::reset() {
  //gets the expected size for the input from the SVM
  m_input_size = 0;
  for (int k=0; k<m_model->l; ++k) {
    svm_node* end = m_model->SV[k];
    while (end->index != -1) {
      if (end->index > (int)m_input_size) m_input_size = end->index;
      ++end;
    }
  }

  //create and reset cache
  m_input_cache.reset(new svm_node[1 + m_input_size]);

  m_input_sub.resize(inputSize());
  m_input_sub = 0.0;
  m_input_div.resize(inputSize());
  m_input_div = 1.0;
}

bob::machine::SupportVector::SupportVector(const std::string& model_file):
  m_model(make_model(model_file.c_str()))
{
  if (!m_model) {
    boost::format s("cannot open model file '%s'");
    s % model_file;
    throw std::runtime_error(s.str());
  }
  reset();
}

bob::machine::SupportVector::SupportVector(bob::io::HDF5File& config):
  m_model()
{
  uint64_t version = 0;
  config.getAttribute(".", "version", version);
  if ( (LIBSVM_VERSION/100) > (version/100) ) {
    //if the major version changes... be aware!
    boost::format m("SVM being loaded from `%s:%s' (created with libsvm-%d) with libsvm-%d. You may want to read the libsvm FAQ at http://www.csie.ntu.edu.tw/~cjlin/libsvm/log to check if there were format changes between these versions. If not, you can safely ignore this warning and even tell us to remove it via our bug tracker: https://github.com/idiap/bob/issues");
    m % config.filename() % config.cwd() % version % LIBSVM_VERSION;
    bob::core::warn << m.str() << std::endl;
  }
  m_model = bob::machine::svm_unpickle(config.readArray<uint8_t,1>("svm_model"));
  reset(); ///< note: has to be done before reading scaling parameters
  config.readArray("input_subtract", m_input_sub);
  config.readArray("input_divide", m_input_div);
}

bob::machine::SupportVector::SupportVector(boost::shared_ptr<svm_model> model)
  : m_model(model)
{
  if (!m_model) {
    throw std::runtime_error("null SVM model cannot be processed");
  }
  reset();
}

bob::machine::SupportVector::~SupportVector() { }

bool bob::machine::SupportVector::supportsProbability() const {
  return svm_check_probability_model(m_model.get());
}

size_t bob::machine::SupportVector::inputSize() const {
  return m_input_size;
}

size_t bob::machine::SupportVector::outputSize() const {
  size_t retval = svm_get_nr_class(m_model.get());
  return (retval == 2)? 1 : retval;
}

size_t bob::machine::SupportVector::numberOfClasses() const {
  return svm_get_nr_class(m_model.get());
}

int bob::machine::SupportVector::classLabel(size_t i) const {

  if (i >= (size_t)svm_get_nr_class(m_model.get())) {
    boost::format s("request for label of class %d in SVM with %d classes is not legal");
    s % (int)i % svm_get_nr_class(m_model.get());
    throw std::runtime_error(s.str());
  }
  return m_model->label[i];

}

bob::machine::SupportVector::svm_t bob::machine::SupportVector::machineType() const {
  return (svm_t)svm_get_svm_type(m_model.get());
}

bob::machine::SupportVector::kernel_t bob::machine::SupportVector::kernelType() const {
  return (kernel_t)m_model->param.kernel_type;
}

int bob::machine::SupportVector::polynomialDegree() const {
  return m_model->param.degree;
}

double bob::machine::SupportVector::gamma() const {
  return m_model->param.gamma;
}

double bob::machine::SupportVector::coefficient0() const {
  return m_model->param.coef0;
}

void bob::machine::SupportVector::setInputSubtraction(const blitz::Array<double,1>& v) {
  if (inputSize() > (size_t)v.extent(0)) {
    boost::format m("mismatch on the input subtraction dimension: expected a vector with **at least** %d positions, but you input %d");
    m % inputSize() % v.extent(0);
    throw std::runtime_error(m.str());
  }
  m_input_sub.reference(bob::core::array::ccopy(v));
}

void bob::machine::SupportVector::setInputDivision(const blitz::Array<double,1>& v) {
  if (inputSize() > (size_t)v.extent(0)) {
    boost::format m("mismatch on the input division dimension: expected a vector with **at least** %d positions, but you input %d");
    m % inputSize() % v.extent(0);
    throw std::runtime_error(m.str());
  }
  m_input_div.reference(bob::core::array::ccopy(v));
}

/**
 * Copies the user input to a locally pre-allocated cache. Apply normalization
 * at the same occasion.
 */
static inline void copy(const blitz::Array<double,1>& input,
    size_t cache_size, boost::shared_array<svm_node>& cache,
    const blitz::Array<double,1>& sub, const blitz::Array<double,1>& div) {

  size_t cur = 0; ///< currently used index

  for (size_t k=0; k<cache_size; ++k) {
    double tmp = (input(k) - sub(k))/div(k);
    if (!tmp) continue;
    cache[cur].index = k+1;
    cache[cur].value = tmp;
    ++cur;
  }

  cache[cur].index = -1; //libsvm detects end of input if index==-1
}

int bob::machine::SupportVector::predictClass_
(const blitz::Array<double,1>& input) const {
  copy(input, m_input_size, m_input_cache, m_input_sub, m_input_div);
  int retval = round(svm_predict(m_model.get(), m_input_cache.get()));
  return retval;
}

int bob::machine::SupportVector::predictClass
(const blitz::Array<double,1>& input) const {

  if ((size_t)input.extent(0) < inputSize()) {
    boost::format s("input for this SVM should have **at least** %d components, but you provided an array with %d elements instead");
    s % inputSize() % input.extent(0);
    throw std::runtime_error(s.str());
  }

  return predictClass_(input);
}

int bob::machine::SupportVector::predictClassAndScores_
(const blitz::Array<double,1>& input,
 blitz::Array<double,1>& scores) const {
  copy(input, m_input_size, m_input_cache, m_input_sub, m_input_div);
#if LIBSVM_VERSION > 290
  int retval = round(svm_predict_values(m_model.get(), m_input_cache.get(), scores.data()));
#else
  svm_predict_values(m_model.get(), m_input_cache.get(), scores.data());
  int retval = round(svm_predict(m_model.get(), m_input_cache.get()));
#endif
  return retval;
}

int bob::machine::SupportVector::predictClassAndScores
(const blitz::Array<double,1>& input,
 blitz::Array<double,1>& scores) const {

  if ((size_t)input.extent(0) < inputSize()) {
    boost::format s("input for this SVM should have **at least** %d components, but you provided an array with %d elements instead");
    s % inputSize() % input.extent(0);
    throw std::runtime_error(s.str());
  }

  if (!bob::core::array::isCContiguous(scores)) {
    throw std::runtime_error("scores output array should be C-style contiguous and what you provided is not");
  }

  size_t N = outputSize();
  size_t size = N < 2 ? 1 : (N*(N-1))/2;
  if ((size_t)scores.extent(0) != size) {
    boost::format s("output scores for this SVM (%d classes) should have %d components, but you provided an array with %d elements instead");
    s % svm_get_nr_class(m_model.get()) % size % scores.extent(0);
    throw std::runtime_error(s.str());
  }

  return predictClassAndScores_(input, scores);
}

int bob::machine::SupportVector::predictClassAndProbabilities_
(const blitz::Array<double,1>& input,
 blitz::Array<double,1>& probabilities) const {
  copy(input, m_input_size, m_input_cache, m_input_sub, m_input_div);
  int retval = round(svm_predict_probability(m_model.get(), m_input_cache.get(), probabilities.data()));
  return retval;
}

int bob::machine::SupportVector::predictClassAndProbabilities
(const blitz::Array<double,1>& input,
 blitz::Array<double,1>& probabilities) const {

  if ((size_t)input.extent(0) < inputSize()) {
    boost::format s("input for this SVM should have **at least** %d components, but you provided an array with %d elements instead");
    s % inputSize() % input.extent(0);
    throw std::runtime_error(s.str());
  }

  if (!supportsProbability()) {
    throw std::runtime_error("this SVM does not support probabilities");
  }

  if (!bob::core::array::isCContiguous(probabilities)) {
    throw std::runtime_error("probabilities output array should be C-style contiguous and what you provided is not");
  }

  if ((size_t)probabilities.extent(0) != outputSize()) {
    boost::format s("output probabilities for this SVM should have %d components, but you provided an array with %d elements instead");
    s % outputSize() % probabilities.extent(0);
    throw std::runtime_error(s.str());
  }

  return predictClassAndProbabilities_(input, probabilities);
}

void bob::machine::SupportVector::save(const std::string& filename) const {
  if (svm_save_model(filename.c_str(), m_model.get())) {
    boost::format s("cannot save SVM model to file '%s'");
    s % filename;
    throw std::runtime_error(s.str());
  }
}

void bob::machine::SupportVector::save(bob::io::HDF5File& config) const {
  config.setArray("svm_model", bob::machine::svm_pickle(m_model));
  config.setArray("input_subtract", m_input_sub);
  config.setArray("input_divide", m_input_div);
  uint64_t version = LIBSVM_VERSION;
  config.setAttribute(".", "version", version);
}
