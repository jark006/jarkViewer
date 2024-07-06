// SPDX-License-Identifier: GPL-2.0-or-later

/*!
  @file    iptc.hpp
  @brief   Encoding and decoding of IPTC data
 */
#ifndef IPTC_HPP_
#define IPTC_HPP_

// *****************************************************************************
#include "exiv2lib_export.h"

// included header files
#include "datasets.hpp"
#include "metadatum.hpp"

// *****************************************************************************
// namespace extensions
namespace Exiv2 {
// *****************************************************************************
// class declarations
class ExifData;

// *****************************************************************************
// class definitions

/*!
  @brief An IPTC metadatum ("dataset"), consisting of an IptcKey and a
         Value and methods to manipulate these.

         This is referred in the standard as a property.
 */
class EXIV2API Iptcdatum : public Metadatum {
 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor for new tags created by an application. The
           %Iptcdatum is created from a key / value pair. %Iptcdatum
           copies (clones) the value if one is provided. Alternatively, a
           program can create an 'empty' %Iptcdatum with only a key and
           set the value using setValue().

    @param key The key of the %Iptcdatum.
    @param pValue Pointer to a %Iptcdatum value.
    @throw Error if the key cannot be parsed and converted
           to a tag number and record id.
   */
  explicit Iptcdatum(const IptcKey& key, const Value* pValue = nullptr);
  //! Copy constructor
  Iptcdatum(const Iptcdatum& rhs);
  //! Destructor
  ~Iptcdatum() override = default;
  //@}

  //! @name Manipulators
  //@{
  //! Assignment operator
  Iptcdatum& operator=(const Iptcdatum& rhs);
  /*!
    @brief Assign \em value to the %Iptcdatum. The type of the new Value
           is set to UShortValue.
   */
  Iptcdatum& operator=(const uint16_t& value);
  /*!
    @brief Assign \em value to the %Iptcdatum.
           Calls setValue(const std::string&).
   */
  Iptcdatum& operator=(const std::string& value);
  /*!
    @brief Assign \em value to the %Iptcdatum.
           Calls setValue(const Value*).
   */
  Iptcdatum& operator=(const Value& value);
  void setValue(const Value* pValue) override;
  /*!
    @brief Set the value to the string \em value, using
           Value::read(const std::string&).
           If the %Iptcdatum does not have a Value yet, then a %Value of
           the correct type for this %Iptcdatum is created. If that
           fails (because of an unknown dataset), a StringValue is
           created. Return 0 if the value was read successfully.
   */
  int setValue(const std::string& value) override;
  //@}

  //! @name Accessors
  //@{
  size_t copy(byte* buf, ByteOrder byteOrder) const override;
  std::ostream& write(std::ostream& os, const ExifData* pMetadata = nullptr) const override;
  /*!
    @brief Return the key of the Iptcdatum. The key is of the form
           '<b>Iptc</b>.recordName.datasetName'. Note however that the key
           is not necessarily unique, i.e., an IptcData object may contain
           multiple metadata with the same key.
   */
  [[nodiscard]] std::string key() const override;
  /*!
     @brief Return the name of the record (deprecated)
     @return record name
   */
  [[nodiscard]] std::string recordName() const;
  /*!
     @brief Return the record id
     @return record id
   */
  [[nodiscard]] uint16_t record() const;
  [[nodiscard]] const char* familyName() const override;
  [[nodiscard]] std::string groupName() const override;
  /*!
     @brief Return the name of the tag (aka dataset)
     @return tag name
   */
  [[nodiscard]] std::string tagName() const override;
  [[nodiscard]] std::string tagLabel() const override;
  [[nodiscard]] std::string tagDesc() const override;
  //! Return the tag (aka dataset) number
  [[nodiscard]] uint16_t tag() const override;
  [[nodiscard]] TypeId typeId() const override;
  [[nodiscard]] const char* typeName() const override;
  [[nodiscard]] size_t typeSize() const override;
  [[nodiscard]] size_t count() const override;
  [[nodiscard]] size_t size() const override;
  [[nodiscard]] std::string toString() const override;
  [[nodiscard]] std::string toString(size_t n) const override;
  [[nodiscard]] int64_t toInt64(size_t n = 0) const override;
  [[nodiscard]] float toFloat(size_t n = 0) const override;
  [[nodiscard]] Rational toRational(size_t n = 0) const override;
  [[nodiscard]] Value::UniquePtr getValue() const override;
  [[nodiscard]] const Value& value() const override;
  //@}

 private:
  // DATA
  IptcKey::UniquePtr key_;  //!< Key
  Value::UniquePtr value_;  //!< Value

};  // class Iptcdatum

//! Container type to hold all metadata
using IptcMetadata = std::vector<Iptcdatum>;

/*!
  @brief A container for IPTC data. This is a top-level class of the %Exiv2 library.

  Provide high-level access to the IPTC data of an image:
  - read IPTC information from JPEG files
  - access metadata through keys and standard C++ iterators
  - add, modify and delete metadata
  - write IPTC data to JPEG files
  - extract IPTC metadata to files, insert from these files
*/
class EXIV2API IptcData {
 public:
  //! IptcMetadata iterator type
  using iterator = IptcMetadata::iterator;
  //! IptcMetadata const iterator type
  using const_iterator = IptcMetadata::const_iterator;

  // Use the compiler generated constructors and assignment operator

  //! @name Manipulators
  //@{
  /*!
    @brief Returns a reference to the %Iptcdatum that is associated with a
           particular \em key. If %IptcData does not already contain such
           an %Iptcdatum, operator[] adds object \em Iptcdatum(key).

    @note  Since operator[] might insert a new element, it can't be a const
           member function.
   */
  Iptcdatum& operator[](const std::string& key);
  /*!
    @brief Add an %Iptcdatum from the supplied key and value pair. This
           method copies (clones) the value. A check for non-repeatable
           datasets is performed.
    @return 0 if successful;<BR>
            6 if the dataset already exists and is not repeatable
   */
  int add(const IptcKey& key, const Value* value);
  /*!
    @brief Add a copy of the Iptcdatum to the IPTC metadata. A check
           for non-repeatable datasets is performed.
    @return 0 if successful;<BR>
           6 if the dataset already exists and is not repeatable;<BR>
   */
  int add(const Iptcdatum& iptcdatum);
  /*!
    @brief Delete the Iptcdatum at iterator position pos, return the
           position of the next Iptcdatum. Note that iterators into
           the metadata, including pos, are potentially invalidated
           by this call.
   */
  iterator erase(iterator pos);
  /*!
    @brief Delete all Iptcdatum instances resulting in an empty container.
   */
  void clear() {
    iptcMetadata_.clear();
  }
  //! Sort metadata by key
  void sortByKey();
  //! Sort metadata by tag (aka dataset)
  void sortByTag();
  //! Begin of the metadata
  iterator begin() {
    return iptcMetadata_.begin();
  }
  //! End of the metadata
  iterator end() {
    return iptcMetadata_.end();
  }
  /*!
    @brief Find the first Iptcdatum with the given key, return an iterator
           to it.
   */
  iterator findKey(const IptcKey& key);
  /*!
    @brief Find the first Iptcdatum with the given record and dataset it,
          return a const iterator to it.
   */
  iterator findId(uint16_t dataset, uint16_t record = IptcDataSets::application2);
  //@}

  //! @name Accessors
  //@{
  //! Begin of the metadata
  [[nodiscard]] const_iterator begin() const {
    return iptcMetadata_.begin();
  }
  //! End of the metadata
  [[nodiscard]] const_iterator end() const {
    return iptcMetadata_.end();
  }
  /*!
    @brief Find the first Iptcdatum with the given key, return a const
           iterator to it.
   */
  [[nodiscard]] const_iterator findKey(const IptcKey& key) const;
  /*!
    @brief Find the first Iptcdatum with the given record and dataset
           number, return a const iterator to it.
   */
  [[nodiscard]] const_iterator findId(uint16_t dataset, uint16_t record = IptcDataSets::application2) const;
  //! Return true if there is no IPTC metadata
  [[nodiscard]] bool empty() const {
    return iptcMetadata_.empty();
  }

  //! Get the number of metadata entries
  [[nodiscard]] size_t count() const {
    return iptcMetadata_.size();
  }

  //! @brief Return the exact size of all contained IPTC metadata
  [[nodiscard]] size_t size() const;

  //! @brief Return the metadata charset name or 0
  [[nodiscard]] const char* detectCharset() const;

  //!  @brief dump iptc formatted binary data (used by printStructure kpsRecursive)
  static void printStructure(std::ostream& out, const Slice<byte*>& bytes, size_t depth);
  //@}

 private:
  // DATA
  IptcMetadata iptcMetadata_;
};  // class IptcData

/*!
  @brief Stateless parser class for IPTC data. Images use this class to
         decode and encode binary IPTC data.
 */
class EXIV2API IptcParser {
 public:
  /*!
    @brief Decode binary IPTC data in IPTC IIM4 format from a buffer \em pData
           of length \em size to the provided metadata container.

    @param iptcData Metadata container to add the decoded IPTC datasets to.
    @param pData    Pointer to the data buffer to read from.
    @param size     Number of bytes in the data buffer.

    @return 0 if successful;<BR>
            5 if the binary IPTC data is invalid or corrupt
   */
  static int decode(IptcData& iptcData, const byte* pData, size_t size);

  /*!
    @brief Encode the IPTC datasets from \em iptcData to a binary representation in IPTC IIM4 format.

    Convert the IPTC datasets to binary format and return it.  Caller owns
    the returned buffer. The copied data follows the IPTC IIM4 standard.

    @return Data buffer containing the binary IPTC data in IPTC IIM4 format.
   */
  static DataBuf encode(const IptcData& iptcData);

 private:
  // Constant data
  static constexpr byte marker_ = 0x1C;  // Dataset marker

};  // class IptcParser

}  // namespace Exiv2

#endif  // #ifndef IPTC_HPP_
