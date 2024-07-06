// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PHOTOSHOP_INT_HPP
#define PHOTOSHOP_INT_HPP

#include "exiv2lib_export.h"

#include "types.hpp"

#include <array>

namespace Exiv2 {
// Forward declarations
class IptcData;

/// @brief Helper class, has methods to deal with %Photoshop "Information Resource Blocks" (IRBs).
struct EXIV2API Photoshop {
  // Todo: Public for now
  static constexpr std::array<const char*, 4> irbId_{"8BIM", "AgHg", "DCSR", "PHUT"};  //!< %Photoshop IRB markers
  static constexpr auto ps3Id_ = "Photoshop 3.0\0";                                    //!< %Photoshop marker
  static constexpr uint16_t iptc_ = 0x0404;                                            //!< %Photoshop IPTC marker
  static constexpr uint16_t preview_ = 0x040c;                                         //!< %Photoshop preview marker

  /// @brief Checks an IRB
  /// @param pPsData  Existing IRB buffer. It is expected to be of size 4.
  /// @return true  if the IRB marker is known
  /// @todo This should be an implementation detail and not exposed in the API. An attacker could try to pass
  ///   a smaller buffer or null pointer.
  static bool isIrb(const byte* pPsData);

  /// @brief Validates all IRBs
  /// @param pPsData        Existing IRB buffer
  /// @param sizePsData     Size of the IRB buffer, may be 0
  /// @return true  if all IRBs are valid;<BR> false otherwise
  static bool valid(const byte* pPsData, size_t sizePsData);

  /// @brief Locates the data for a %Photoshop tag in a %Photoshop formatted memory buffer.
  /// Operates on raw data to simplify reuse.
  /// @param pPsData Pointer to buffer containing entire payload of %Photoshop formatted data (from APP13 Jpeg segment)
  /// @param sizePsData Size in bytes of pPsData.
  /// @param psTag %Tag number of the block to look for.
  /// @param record Output value that is set to the start of the data block within pPsData (may not be null).
  /// @param sizeHdr Output value that is set to the size of the header within the data block pointed to by record
  ///   (may not be null).
  /// @param sizeData Output value that is set to the size of the actual data within the data block pointed to by record
  /// (may not be null).
  /// @return 0 if successful;<BR>
  ///   3 if no data for psTag was found in pPsData;<BR>
  ///  -2 if the pPsData buffer does not contain valid data.
  static int locateIrb(const byte* pPsData, size_t sizePsData, uint16_t psTag, const byte** record, uint32_t& sizeHdr,
                       uint32_t& sizeData);

  /// @brief Forwards to locateIrb() with \em psTag = \em iptc_
  static int locateIptcIrb(const byte* pPsData, size_t sizePsData, const byte** record, uint32_t& sizeHdr,
                           uint32_t& sizeData);

  /// @brief Forwards to locatePreviewIrb() with \em psTag = \em preview_
  static int locatePreviewIrb(const byte* pPsData, size_t sizePsData, const byte** record, uint32_t& sizeHdr,
                              uint32_t& sizeData);

  /// @brief Set the new IPTC IRB, keeps existing IRBs but removes the IPTC block if there is no new IPTC data to write.
  /// @param pPsData    Existing IRB buffer
  /// @param sizePsData Size of the IRB buffer, may be 0
  /// @param iptcData   Iptc data to embed, may be empty
  /// @return A data buffer containing the new IRB buffer, may have 0 size
  static DataBuf setIptcIrb(const byte* pPsData, size_t sizePsData, const IptcData& iptcData);
};
}  // namespace Exiv2

#endif  // PHOTOSHOP_INT_HPP
