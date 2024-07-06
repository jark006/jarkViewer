// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef TAGS_HPP_
#define TAGS_HPP_

// *****************************************************************************
#include "exiv2lib_export.h"

// included header files
#include "metadatum.hpp"

// *****************************************************************************
// namespace extensions
namespace Exiv2 {
// *****************************************************************************
// class declarations
class ExifData;
class ExifKey;
class Value;
struct TagInfo;

// *****************************************************************************
// type definitions

//! Type for a function pointer for functions interpreting the tag value
using PrintFct = std::ostream& (*)(std::ostream&, const Value&, const ExifData* pExifData);
//! A function returning a tag list.
using TagListFct = const TagInfo* (*)();

// *****************************************************************************
// class definitions

//! Type to specify the IFD to which a metadata belongs
enum class IfdId : uint32_t {
  ifdIdNotSet,
  ifd0Id,
  ifd1Id,
  ifd2Id,
  ifd3Id,
  exifId,
  gpsId,
  iopId,
  mpfId,
  subImage1Id,
  subImage2Id,
  subImage3Id,
  subImage4Id,
  subImage5Id,
  subImage6Id,
  subImage7Id,
  subImage8Id,
  subImage9Id,
  subThumb1Id,
  panaRawId,
  mnId,
  canonId,
  canonAfCId,
  canonAfMiAdjId,
  canonAmId,
  canonAsId,
  canonCbId,
  canonCiId,
  canonCsId,
  canonFilId,
  canonFlId,
  canonHdrId,
  canonLeId,
  canonMeId,
  canonMoID,
  canonMvId,
  canonRawBId,
  canonSiId,
  canonCfId,
  canonContrastId,
  canonFcd1Id,
  canonFcd2Id,
  canonFcd3Id,
  canonLiOpId,
  canonMyColorID,
  canonPiId,
  canonPaId,
  canonTiId,
  canonFiId,
  canonPrId,
  canonPreID,
  canonVigCorId,
  canonVigCor2Id,
  canonWbId,
  casioId,
  casio2Id,
  fujiId,
  minoltaId,
  minoltaCs5DId,
  minoltaCs7DId,
  minoltaCsOldId,
  minoltaCsNewId,
  nikon1Id,
  nikon2Id,
  nikon3Id,
  nikonPvId,
  nikonVrId,
  nikonPcId,
  nikonWtId,
  nikonIiId,
  nikonAfId,
  nikonAf21Id,
  nikonAf22Id,
  nikonAFTId,
  nikonFiId,
  nikonMeId,
  nikonFl1Id,
  nikonFl2Id,
  nikonFl3Id,
  nikonFl6Id,
  nikonFl7Id,
  nikonSi1Id,
  nikonSi2Id,
  nikonSi3Id,
  nikonSi4Id,
  nikonSi5Id,
  nikonSi6Id,
  nikonLd1Id,
  nikonLd2Id,
  nikonLd3Id,
  nikonLd4Id,
  nikonCb1Id,
  nikonCb2Id,
  nikonCb2aId,
  nikonCb2bId,
  nikonCb3Id,
  nikonCb4Id,
  olympusId,
  olympus2Id,
  olympusCsId,
  olympusEqId,
  olympusRdId,
  olympusRd2Id,
  olympusIpId,
  olympusFiId,
  olympusFe1Id,
  olympusFe2Id,
  olympusFe3Id,
  olympusFe4Id,
  olympusFe5Id,
  olympusFe6Id,
  olympusFe7Id,
  olympusFe8Id,
  olympusFe9Id,
  olympusRiId,
  panasonicId,
  pentaxId,
  pentaxDngId,
  samsung2Id,
  samsungPvId,
  samsungPwId,
  sigmaId,
  sony1Id,
  sony2Id,
  sonyMltId,
  sony1CsId,
  sony1Cs2Id,
  sony2CsId,
  sony2Cs2Id,
  sony2FpId,
  sonyMisc1Id,
  sonyMisc2bId,
  sonyMisc3cId,
  sonySInfo1Id,
  sony2010eId,
  sony1MltCs7DId,
  sony1MltCsOldId,
  sony1MltCsNewId,
  sony1MltCsA100Id,
  tagInfoMvId,
  lastId,
  ignoreId = lastId
};

inline std::ostream& operator<<(std::ostream& os, IfdId id) {
  return os << static_cast<int>(id);
}

//! The details of an Exif group. Groups include IFDs and binary arrays.
struct EXIV2API GroupInfo {
  using GroupName = std::string;
  bool operator==(IfdId ifdId) const;                 //!< Comparison operator for IFD id
  bool operator==(const GroupName& groupName) const;  //!< Comparison operator for group name
  IfdId ifdId_;                                       //!< IFD id
  const char* ifdName_;                               //!< IFD name
  const char* groupName_;                             //!< Group name, unique for each group.
  TagListFct tagList_;                                //!< Tag list
};

/*!
  @brief Section identifiers to logically group tags. A section consists
         of nothing more than a name, based on the Exif standard.
 */
enum class SectionId {
  sectionIdNotSet,
  imgStruct,     // 4.6.4 A
  recOffset,     // 4.6.4 B
  imgCharacter,  // 4.6.4 C
  otherTags,     // 4.6.4 D
  exifFormat,    // 4.6.3
  exifVersion,   // 4.6.5 A
  imgConfig,     // 4.6.5 C
  userInfo,      // 4.6.5 D
  relatedFile,   // 4.6.5 E
  dateTime,      // 4.6.5 F
  captureCond,   // 4.6.5 G
  gpsTags,       // 4.6.6
  iopTags,       // 4.6.7
  mpfTags,
  makerTags,  // MakerNote
  dngTags,    // DNG Spec
  panaRaw,
  tiffEp,  // TIFF-EP Spec
  tiffPm6,
  adobeOpi,
  lastSectionId
};

//! Tag information
struct EXIV2API TagInfo {
  uint16_t tag_;         //!< Tag
  const char* name_;     //!< One word tag label
  const char* title_;    //!< Tag title
  const char* desc_;     //!< Short tag description
  IfdId ifdId_;          //!< Link to the (preferred) IFD
  SectionId sectionId_;  //!< Section id
  TypeId typeId_;        //!< Type id
  int16_t count_;        //!< The number of values (not bytes!), 0=any, -1=count not known.
  PrintFct printFct_;    //!< Pointer to tag print function
};                       // struct TagInfo

//! Access to Exif group and tag lists and misc. tag reference methods, implemented as a static class.
class EXIV2API ExifTags {
 public:
  //! Return read-only list of built-in groups
  static const GroupInfo* groupList();
  //! Return read-only list of built-in \em groupName tags.
  static const TagInfo* tagList(const std::string& groupName);
  //! Print a list of all standard Exif tags to output stream
  static void taglist(std::ostream& os);
  //! Print the list of tags for \em groupName
  static void taglist(std::ostream& os, const std::string& groupName);

  //! Return the name of the section for an Exif \em key.
  static const char* sectionName(const ExifKey& key);
  //! Return the default number of components (not bytes!) \em key has. (0=any, -1=count not known)
  static uint16_t defaultCount(const ExifKey& key);
  //! Return the name of the IFD for the group.
  static const char* ifdName(const std::string& groupName);

  /*!
    @brief Return true if \em groupName is a makernote group.
  */
  static bool isMakerGroup(const std::string& groupName);
  /*!
    @brief Return true if \em groupName is a TIFF or Exif IFD, else false.
           This is used to differentiate between standard Exif IFDs
           and IFDs associated with the makernote.
  */
  static bool isExifGroup(const std::string& groupName);

};  // class ExifTags

/*!
  @brief Concrete keys for Exif metadata and access to Exif tag reference data.
 */
class EXIV2API ExifKey : public Key {
 public:
  //! Shortcut for an %ExifKey auto pointer.
  using UniquePtr = std::unique_ptr<ExifKey>;

  //! @name Creators
  //@{
  /*!
    @brief Constructor to create an Exif key from a key string.

    @param key The key string.
    @throw Error if the first part of the key is not '<b>Exif</b>' or
           the remaining parts of the key cannot be parsed and
           converted to a group name and tag name.
  */
  explicit ExifKey(const std::string& key);
  /*!
    @brief Constructor to create an Exif key from the tag number and
           group name.
    @param tag The tag value
    @param groupName The name of the group, i.e., the second part of
           the Exif key.
    @throw Error if the key cannot be constructed from the tag number
           and group name.
   */
  ExifKey(uint16_t tag, const std::string& groupName);
  /*!
    @brief Constructor to create an Exif key from a TagInfo instance.
    @param ti The TagInfo instance
    @throw Error if the key cannot be constructed from the tag number
           and group name.
   */
  explicit ExifKey(const TagInfo& ti);

  //! Copy constructor
  ExifKey(const ExifKey& rhs);
  //! Destructor
  ~ExifKey() override;
  //@}

  //! @name Manipulators
  //@{
  /*!
    @brief Assignment operator.
   */
  ExifKey& operator=(const ExifKey& rhs);
  //! Set the index.
  void setIdx(int idx) const;
  //@}

  //! @name Accessors
  //@{
  [[nodiscard]] std::string key() const override;
  [[nodiscard]] const char* familyName() const override;
  [[nodiscard]] std::string groupName() const override;
  //! Return the IFD id. (Do not use, this is meant for library internal use.)
  [[nodiscard]] IfdId ifdId() const;
  [[nodiscard]] std::string tagName() const override;
  [[nodiscard]] uint16_t tag() const override;
  [[nodiscard]] std::string tagLabel() const override;
  [[nodiscard]] std::string tagDesc() const override;
  //! Return the default type id for this tag.
  [[nodiscard]] TypeId defaultTypeId() const;  // Todo: should be in the base class

  [[nodiscard]] UniquePtr clone() const;
  //! Return the index (unique id of this key within the original Exif data, 0 if not set)
  [[nodiscard]] int idx() const;
  //@}

 private:
  //! Internal virtual copy constructor.
  [[nodiscard]] ExifKey* clone_() const override;

  // Pimpl idiom
  struct Impl;
  std::unique_ptr<Impl> p_;

};  // class ExifKey

// *****************************************************************************
// free functions

//! Output operator for TagInfo
EXIV2API std::ostream& operator<<(std::ostream& os, const TagInfo& ti);

}  // namespace Exiv2

#endif  // #ifndef TAGS_HPP_
