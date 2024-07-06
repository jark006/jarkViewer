// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef BASICIO_HPP_
#define BASICIO_HPP_

// *****************************************************************************
#include "exiv2lib_export.h"

// included header files
#include "error.hpp"
#include "types.hpp"

// + standard includes
#include <memory>

// The way to handle data from stdin or data uri path. If EXV_XPATH_MEMIO = 1,
// it uses MemIo. Otherwises, it uses FileIo.
#ifndef EXV_XPATH_MEMIO
#define EXV_XPATH_MEMIO 0
#endif

// *****************************************************************************
// namespace extensions
namespace Exiv2 {
// *****************************************************************************
// class definitions

/*!
  @brief An interface for simple binary IO.

  Designed to have semantics and names similar to those of C style FILE*
  operations. Subclasses should all behave the same so that they can be
  interchanged.
 */
class EXIV2API BasicIo {
 public:
  //! BasicIo auto_ptr type
  using UniquePtr = std::unique_ptr<BasicIo>;

  //! Seek starting positions
  enum Position { beg, cur, end };

  //! @name Creators
  //@{
  //! Destructor
  virtual ~BasicIo() = default;
  //@}

  //! @name Manipulators
  //@{
  /*!
    @brief Open the IO source using the default access mode. The
        default mode should allow for reading and writing.

    This method can also be used to "reopen" an IO source which will
    flush any unwritten data and reset the IO position to the start.
    Subclasses may provide custom methods to allow for
    opening IO sources differently.

    @return 0 if successful;<BR>
        Nonzero if failure.
   */
  virtual int open() = 0;

  /*!
    @brief Close the IO source. After closing a BasicIo instance can not
        be read or written. Closing flushes any unwritten data. It is
        safe to call close on a closed instance.
    @return 0 if successful;<BR>
        Nonzero if failure.
   */
  virtual int close() = 0;
  /*!
    @brief Write data to the IO source. Current IO position is advanced
        by the number of bytes written.
    @param data Pointer to data. Data must be at least \em wcount
        bytes long
    @param wcount Number of bytes to be written.
    @return Number of bytes written to IO source successfully;<BR>
        0 if failure;
   */
  virtual size_t write(const byte* data, size_t wcount) = 0;
  /*!
    @brief Write data that is read from another BasicIo instance to
        the IO source. Current IO position is advanced by the number
        of bytes written.
    @param src Reference to another BasicIo instance. Reading start
        at the source's current IO position
    @return Number of bytes written to IO source successfully;<BR>
        0 if failure;
   */
  virtual size_t write(BasicIo& src) = 0;
  /*!
    @brief Write one byte to the IO source. Current IO position is
        advanced by one byte.
    @param data The single byte to be written.
    @return The value of the byte written if successful;<BR>
        EOF if failure;
   */
  virtual int putb(byte data) = 0;
  /*!
    @brief Read data from the IO source. Reading starts at the current
        IO position and the position is advanced by the number of bytes
        read.
    @param rcount Maximum number of bytes to read. Fewer bytes may be
        read if \em rcount bytes are not available.
    @return DataBuf instance containing the bytes read. Use the
        DataBuf::size_ member to find the number of bytes read.
        DataBuf::size_ will be 0 on failure.
   */
  virtual DataBuf read(size_t rcount) = 0;
  /*!
    @brief Read data from the IO source. Reading starts at the current
        IO position and the position is advanced by the number of bytes
        read.
    @param buf Pointer to a block of memory into which the read data
        is stored. The memory block must be at least \em rcount bytes
        long.
    @param rcount Maximum number of bytes to read. Fewer bytes may be
        read if \em rcount bytes are not available.
    @return Number of bytes read from IO source successfully;<BR>
        0 if failure;
   */
  virtual size_t read(byte* buf, size_t rcount) = 0;
  /*!
    @brief Safe version of `read()` that checks for errors and throws
        an exception if the read was unsuccessful.
    @param buf Pointer to a block of memory into which the read data
        is stored. The memory block must be at least \em rcount bytes
        long.
    @param rcount Maximum number of bytes to read. Fewer bytes may be
        read if \em rcount bytes are not available.
    @param err Error code to use if an exception is thrown.
   */
  void readOrThrow(byte* buf, size_t rcount, ErrorCode err = ErrorCode::kerCorruptedMetadata);
  /*!
    @brief Read one byte from the IO source. Current IO position is
        advanced by one byte.
    @return The byte read from the IO source if successful;<BR>
        EOF if failure;
   */
  virtual int getb() = 0;
  /*!
    @brief Remove all data from this object's IO source and then transfer
        data from the \em src BasicIo object into this object.

    The source object is invalidated by this operation and should not be
    used after this method returns. This method exists primarily to
    be used with the BasicIo::temporary() method.

    @param src Reference to another BasicIo instance. The entire contents
        of src are transferred to this object. The \em src object is
        invalidated by the method.
    @throw Error In case of failure
   */
  virtual void transfer(BasicIo& src) = 0;
  /*!
    @brief Move the current IO position.
    @param offset Number of bytes to move the position relative
        to the starting position specified by \em pos
    @param pos Position from which the seek should start
    @return 0 if successful;<BR>
        Nonzero if failure;
   */
  virtual int seek(int64_t offset, Position pos) = 0;

  /*!
    @brief Safe version of `seek()` that checks for errors and throws
        an exception if the seek was unsuccessful.
    @param offset Number of bytes to move the position relative
        to the starting position specified by \em pos
    @param pos Position from which the seek should start
    @param err Error code to use if an exception is thrown.
   */
  void seekOrThrow(int64_t offset, Position pos, ErrorCode err);

  /*!
    @brief Direct access to the IO data. For files, this is done by
           mapping the file into the process's address space; for memory
           blocks, this allows direct access to the memory block.
    @param isWriteable Set to true if the mapped area should be writeable
           (default is false).
    @return A pointer to the mapped area.
    @throw Error In case of failure.
   */
  virtual byte* mmap(bool isWriteable = false) = 0;
  /*!
    @brief Remove a mapping established with mmap(). If the mapped area
           is writeable, this ensures that changes are written back.
    @return 0 if successful;<BR>
            Nonzero if failure;
   */
  virtual int munmap() = 0;

  //@}

  //! @name Accessors
  //@{
  /*!
    @brief Get the current IO position.
    @return Offset from the start of IO
   */
  [[nodiscard]] virtual size_t tell() const = 0;
  /*!
    @brief Get the current size of the IO source in bytes.
    @return Size of the IO source in bytes;<BR>
           -1 if failure;
   */
  [[nodiscard]] virtual size_t size() const = 0;
  //! Returns true if the IO source is open, otherwise false.
  [[nodiscard]] virtual bool isopen() const = 0;
  //! Returns 0 if the IO source is in a valid state, otherwise nonzero.
  [[nodiscard]] virtual int error() const = 0;
  //! Returns true if the IO position has reached the end, otherwise false.
  [[nodiscard]] virtual bool eof() const = 0;
  /*!
    @brief Return the path to the IO resource. Often used to form
        comprehensive error messages where only a BasicIo instance is
        available.
   */
  [[nodiscard]] virtual const std::string& path() const noexcept = 0;

  /*!
    @brief Mark all the bNone blocks to bKnow. This avoids allocating memory
      for parts of the file that contain image-date (non-metadata/pixel data)

    @note This method should be only called after the concerned data (metadata)
          are all downloaded from the remote file to memory.
   */
  virtual void populateFakeData() = 0;

  /*!
    @brief this is allocated and populated by mmap()
   */
  byte* bigBlock_{};

  //@}
};  // class BasicIo

/*!
  @brief Utility class that closes a BasicIo instance upon destruction.
      Meant to be used as a stack variable in functions that need to
      ensure BasicIo instances get closed. Useful when functions return
      errors from many locations.
 */
class EXIV2API IoCloser {
 public:
  //! @name Creators
  //@{
  //! Constructor, takes a BasicIo reference
  explicit IoCloser(BasicIo& bio) : bio_(bio) {
  }
  //! Destructor, closes the BasicIo reference
  virtual ~IoCloser() {
    close();
  }
  //@}

  //! @name Manipulators
  //@{
  //! Close the BasicIo if it is open
  void close() {
    if (bio_.isopen())
      bio_.close();
  }
  //@}

  // DATA
  //! The BasicIo reference
  BasicIo& bio_;

  // Not implemented
  //! Copy constructor
  IoCloser(const IoCloser&) = delete;
  //! Assignment operator
  IoCloser& operator=(const IoCloser&) = delete;
};  // class IoCloser

/*!
  @brief Provides binary file IO by implementing the BasicIo
      interface.
 */
class EXIV2API FileIo : public BasicIo {
 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor that accepts the file path on which IO will be
        performed. The constructor does not open the file, and
        therefore never fails.
    @param path The full path of a file
   */
  explicit FileIo(const std::string& path);

  //! Destructor. Flushes and closes an open file.
  ~FileIo() override;
  //@}

  //! @name Manipulators
  //@{
  /*!
    @brief Open the file using the specified mode.

    This method can also be used to "reopen" a file which will flush any
    unwritten data and reset the IO position to the start. Although
    files can be opened in binary or text mode, this class has
    only been tested carefully in binary mode.

    @param mode Specified that type of access allowed on the file.
        Valid values match those of the C fopen command exactly.
    @return 0 if successful;<BR>
        Nonzero if failure.
   */
  int open(const std::string& mode);
  /*!
    @brief Open the file using the default access mode of "rb".
        This method can also be used to "reopen" a file which will flush
        any unwritten data and reset the IO position to the start.
    @return 0 if successful;<BR>
        Nonzero if failure.
   */
  int open() override;
  /*!
    @brief Flush and unwritten data and close the file . It is
        safe to call close on an already closed instance.
    @return 0 if successful;<BR>
           Nonzero if failure;
   */
  int close() override;
  /*!
    @brief Write data to the file. The file position is advanced
        by the number of bytes written.
    @param data Pointer to data. Data must be at least \em wcount
        bytes long
    @param wcount Number of bytes to be written.
    @return Number of bytes written to the file successfully;<BR>
           0 if failure;
   */
  size_t write(const byte* data, size_t wcount) override;
  /*!
    @brief Write data that is read from another BasicIo instance to
        the file. The file position is advanced by the number
        of bytes written.
    @param src Reference to another BasicIo instance. Reading start
        at the source's current IO position
    @return Number of bytes written to the file successfully;<BR>
           0 if failure;
   */
  size_t write(BasicIo& src) override;
  /*!
    @brief Write one byte to the file. The file position is
        advanced by one byte.
    @param data The single byte to be written.
    @return The value of the byte written if successful;<BR>
           EOF if failure;
   */
  int putb(byte data) override;
  /*!
    @brief Read data from the file. Reading starts at the current
        file position and the position is advanced by the number of
        bytes read.
    @param rcount Maximum number of bytes to read. Fewer bytes may be
        read if \em rcount bytes are not available.
    @return DataBuf instance containing the bytes read. Use the
          DataBuf::size_ member to find the number of bytes read.
          DataBuf::size_ will be 0 on failure.
   */
  DataBuf read(size_t rcount) override;
  /*!
    @brief Read data from the file. Reading starts at the current
        file position and the position is advanced by the number of
        bytes read.
    @param buf Pointer to a block of memory into which the read data
        is stored. The memory block must be at least \em rcount bytes
        long.
    @param rcount Maximum number of bytes to read. Fewer bytes may be
        read if \em rcount bytes are not available.
    @return Number of bytes read from the file successfully;<BR>
           0 if failure;
   */
  size_t read(byte* buf, size_t rcount) override;
  /*!
    @brief Read one byte from the file. The file position is
        advanced by one byte.
    @return The byte read from the file if successful;<BR>
           EOF if failure;
   */
  int getb() override;
  /*!
    @brief Remove the contents of the file and then transfer data from
        the \em src BasicIo object into the empty file.

    This method is optimized to simply rename the source file if the
    source object is another FileIo instance. The source BasicIo object
    is invalidated by this operation and should not be used after this
    method returns. This method exists primarily to be used with
    the BasicIo::temporary() method.

    @note If the caller doesn't have permissions to write to the file,
        an exception is raised and \em src is deleted.

    @param src Reference to another BasicIo instance. The entire contents
        of src are transferred to this object. The \em src object is
        invalidated by the method.
    @throw Error In case of failure
   */
  void transfer(BasicIo& src) override;

  int seek(int64_t offset, Position pos) override;

  /*!
    @brief Map the file into the process's address space. The file must be
           open before mmap() is called. If the mapped area is writeable,
           changes may not be written back to the underlying file until
           munmap() is called. The pointer is valid only as long as the
           FileIo object exists.
    @param isWriteable Set to true if the mapped area should be writeable
           (default is false).
    @return A pointer to the mapped area.
    @throw Error In case of failure.
   */
  byte* mmap(bool isWriteable = false) override;
  /*!
    @brief Remove a mapping established with mmap(). If the mapped area is
           writeable, this ensures that changes are written back to the
           underlying file.
    @return 0 if successful;<BR>
            Nonzero if failure;
   */
  int munmap() override;
  /*!
    @brief close the file source and set a new path.
   */
  virtual void setPath(const std::string& path);

  //@}
  //! @name Accessors
  //@{
  /*!
    @brief Get the current file position.
    @return Offset from the start of the file
   */
  [[nodiscard]] size_t tell() const override;
  /*!
    @brief Flush any buffered writes and get the current file size
        in bytes.
    @return Size of the file in bytes;<BR>
           -1 if failure;
   */
  [[nodiscard]] size_t size() const override;
  //! Returns true if the file is open, otherwise false.
  [[nodiscard]] bool isopen() const override;
  //! Returns 0 if the file is in a valid state, otherwise nonzero.
  [[nodiscard]] int error() const override;
  //! Returns true if the file position has reached the end, otherwise false.
  [[nodiscard]] bool eof() const override;
  //! Returns the path of the file
  [[nodiscard]] const std::string& path() const noexcept override;

  /*!
    @brief Mark all the bNone blocks to bKnow. This avoids allocating memory
      for parts of the file that contain image-date (non-metadata/pixel data)

    @note This method should be only called after the concerned data (metadata)
          are all downloaded from the remote file to memory.
   */
  void populateFakeData() override;
  //@}

  // NOT IMPLEMENTED
  //! Copy constructor
  FileIo(const FileIo&) = delete;
  //! Assignment operator
  FileIo& operator=(const FileIo&) = delete;

 private:
  // Pimpl idiom
  class Impl;
  std::unique_ptr<Impl> p_;

};  // class FileIo

/*!
  @brief Provides binary IO on blocks of memory by implementing the BasicIo
      interface. A copy-on-write implementation ensures that the data passed
      in is only copied when necessary, i.e., as soon as data is written to
      the MemIo. The original data is only used for reading. If writes are
      performed, the changed data can be retrieved using the read methods
      (since the data used in construction is never modified).

  @note If read only usage of this class is common, it might be worth
      creating a specialized readonly class or changing this one to
      have a readonly mode.
 */
class EXIV2API MemIo : public BasicIo {
 public:
  //! @name Creators
  //@{
  //! Default constructor that results in an empty object
  MemIo();
  /*!
    @brief Constructor that accepts a block of memory. A copy-on-write
        algorithm allows read operations directly from the original data
        and will create a copy of the buffer on the first write operation.
    @param data Pointer to data. Data must be at least \em size bytes long
    @param size Number of bytes to copy.
   */
  MemIo(const byte* data, size_t size);
  //! Destructor. Releases all managed memory
  ~MemIo() override;
  //@}

  //! @name Manipulators
  //@{
  /*!
    @brief Memory IO is always open for reading and writing. This method
           therefore only resets the IO position to the start.

    @return 0
   */
  int open() override;
  /*!
    @brief Does nothing on MemIo objects.
    @return 0
   */
  int close() override;
  /*!
    @brief Write data to the memory block. If needed, the size of the
        internal memory block is expanded. The IO position is advanced
        by the number of bytes written.
    @param data Pointer to data. Data must be at least \em wcount
        bytes long
    @param wcount Number of bytes to be written.
    @return Number of bytes written to the memory block successfully;<BR>
           0 if failure;
   */
  size_t write(const byte* data, size_t wcount) override;
  /*!
    @brief Write data that is read from another BasicIo instance to
        the memory block. If needed, the size of the internal memory
        block is expanded. The IO position is advanced by the number
        of bytes written.
    @param src Reference to another BasicIo instance. Reading start
        at the source's current IO position
    @return Number of bytes written to the memory block successfully;<BR>
           0 if failure;
   */
  size_t write(BasicIo& src) override;
  /*!
    @brief Write one byte to the memory block. The IO position is
        advanced by one byte.
    @param data The single byte to be written.
    @return The value of the byte written if successful;<BR>
           EOF if failure;
   */
  int putb(byte data) override;
  /*!
    @brief Read data from the memory block. Reading starts at the current
        IO position and the position is advanced by the number of
        bytes read.
    @param rcount Maximum number of bytes to read. Fewer bytes may be
        read if \em rcount bytes are not available.
    @return DataBuf instance containing the bytes read. Use the
          DataBuf::size_ member to find the number of bytes read.
          DataBuf::size_ will be 0 on failure.
   */
  DataBuf read(size_t rcount) override;
  /*!
    @brief Read data from the memory block. Reading starts at the current
        IO position and the position is advanced by the number of
        bytes read.
    @param buf Pointer to a block of memory into which the read data
        is stored. The memory block must be at least \em rcount bytes
        long.
    @param rcount Maximum number of bytes to read. Fewer bytes may be
        read if \em rcount bytes are not available.
    @return Number of bytes read from the memory block successfully;<BR>
           0 if failure;
   */
  size_t read(byte* buf, size_t rcount) override;
  /*!
    @brief Read one byte from the memory block. The IO position is
        advanced by one byte.
    @return The byte read from the memory block if successful;<BR>
           EOF if failure;
   */
  int getb() override;
  /*!
    @brief Clear the memory block and then transfer data from
        the \em src BasicIo object into a new block of memory.

    This method is optimized to simply swap memory block if the source
    object is another MemIo instance. The source BasicIo instance
    is invalidated by this operation and should not be used after this
    method returns. This method exists primarily to be used with
    the BasicIo::temporary() method.

    @param src Reference to another BasicIo instance. The entire contents
        of src are transferred to this object. The \em src object is
        invalidated by the method.
    @throw Error In case of failure
   */
  void transfer(BasicIo& src) override;

  int seek(int64_t offset, Position pos) override;

  /*!
    @brief Allow direct access to the underlying data buffer. The buffer
           is not protected against write access in any way, the argument
           is ignored.
    @note  The application must ensure that the memory pointed to by the
           returned pointer remains valid and allocated as long as the
           MemIo object exists.
   */
  byte* mmap(bool /*isWriteable*/ = false) override;
  int munmap() override;
  //@}

  //! @name Accessors
  //@{
  /*!
    @brief Get the current IO position.
    @return Offset from the start of the memory block
   */
  [[nodiscard]] size_t tell() const override;
  /*!
    @brief Get the current memory buffer size in bytes.
    @return Size of the in memory data in bytes;<BR>
           -1 if failure;
   */
  [[nodiscard]] size_t size() const override;
  //! Always returns true
  [[nodiscard]] bool isopen() const override;
  //! Always returns 0
  [[nodiscard]] int error() const override;
  //! Returns true if the IO position has reached the end, otherwise false.
  [[nodiscard]] bool eof() const override;
  //! Returns a dummy path, indicating that memory access is used
  [[nodiscard]] const std::string& path() const noexcept override;

  /*!
    @brief Mark all the bNone blocks to bKnow. This avoids allocating memory
      for parts of the file that contain image-date (non-metadata/pixel data)

    @note This method should be only called after the concerned data (metadata)
          are all downloaded from the remote file to memory.
   */
  void populateFakeData() override;

  //@}

  // NOT IMPLEMENTED
  //! Copy constructor
  MemIo(const MemIo&) = delete;
  //! Assignment operator
  MemIo& operator=(const MemIo&) = delete;

 private:
  // Pimpl idiom
  class Impl;
  std::unique_ptr<Impl> p_;

};  // class MemIo

/*!
  @brief Provides binary IO for the data from stdin and data uri path.
 */
#if EXV_XPATH_MEMIO
class EXIV2API XPathIo : public MemIo {
 public:
  //! @name Creators
  //@{
  //! Default constructor
  XPathIo(const std::string& path);
  //@}
 private:
  /*!
      @brief Read data from stdin and write the data to memory.
      @throw Error if it can't convert stdin to binary.
   */
  void ReadStdin();
  /*!
      @brief Read the data from data uri path and write the data to memory.
      @param path The data uri.
      @throw Error if no base64 data in path.
   */
  void ReadDataUri(const std::string& path);
};  // class XPathIo
#else
class EXIV2API XPathIo : public FileIo {
 public:
  /*!
      @brief The extension of the temporary file which is created when getting input data
              to read metadata. This file will be deleted in destructor.
  */
  static constexpr auto TEMP_FILE_EXT = ".exiv2_temp";
  /*!
      @brief The extension of the generated file which is created when getting input data
              to add or modify the metadata.
  */
  static constexpr auto GEN_FILE_EXT = ".exiv2";

  //! @name Creators
  //@{
  //! Default constructor that reads data from stdin/data uri path and writes them to the temp file.
  explicit XPathIo(const std::string& orgPath);

  //! Destructor. Releases all managed memory and removes the temp file.
  ~XPathIo() override;
  //@}

  XPathIo(const XPathIo&) = delete;
  XPathIo& operator=(const XPathIo&) = delete;

  //! @name Manipulators
  //@{
  /*!
      @brief Change the name of the temp file and make it untemporary before
              calling the method of superclass FileIo::transfer.
   */
  void transfer(BasicIo& src) override;

  //@}

  //! @name Static methods
  //@{
  /*!
      @brief Read the data from stdin/data uri path and write them to the file.
      @param orgPath It equals "-" if the input data's from stdin. Otherwise, it's data uri path.
      @return the name of the new file.
      @throw Error if it fails.
   */
  static std::string writeDataToFile(const std::string& orgPath);
  //@}

 private:
  // True if the file is a temporary file and it should be deleted in destructor.
  bool isTemp_{true};
  std::string tempFilePath_;
};  // class XPathIo
#endif

/*!
    @brief Provides remote binary file IO by implementing the BasicIo interface. This is an
        abstract class. The logics for remote access are implemented in HttpIo, CurlIo, SshIo which
        are the derived classes of RemoteIo.
*/
class EXIV2API RemoteIo : public BasicIo {
 public:
  //! Destructor. Releases all managed memory.
  RemoteIo();
  ~RemoteIo() override;
  //@}

  RemoteIo(const RemoteIo&) = delete;
  RemoteIo& operator=(const RemoteIo&) = delete;

  //! @name Manipulators
  //@{
  /*!
    @brief Connect to the remote server, get the size of the remote file and
      allocate the array of blocksMap.

      If the blocksMap is already allocated (this method has been called before),
      it just reset IO position to the start and does not flush the old data.
    @return 0 if successful;<BR>
        Nonzero if failure.
   */
  int open() override;

  /*!
    @brief Reset the IO position to the start. It does not release the data.
    @return 0 if successful;<BR>
        Nonzero if failure.
   */
  int close() override;
  /*!
    @brief Not support this method.
    @return 0 means failure
   */
  size_t write(const byte* data, size_t wcount) override;
  /*!
    @brief Write data that is read from another BasicIo instance to the remote file.

    The write access is done in an efficient way. It only sends the range of different
    bytes between the current data and BasicIo instance to the remote machine.

    @param src Reference to another BasicIo instance. Reading start
        at the source's current IO position
    @return The size of BasicIo instance;<BR>
           0 if failure;
    @throw Error In case of failure

    @note The write access is only supported by http, https, ssh.
   */
  size_t write(BasicIo& src) override;

  /*!
   @brief Not support
   @return 0 means failure
  */
  int putb(byte data) override;
  /*!
    @brief Read data from the memory blocks. Reading starts at the current
        IO position and the position is advanced by the number of
        bytes read.
        If the memory blocks are not populated (False), it will connect to server
        and populate the data to memory blocks.
    @param rcount Maximum number of bytes to read. Fewer bytes may be
        read if \em rcount bytes are not available.
    @return DataBuf instance containing the bytes read. Use the
          DataBuf::size_ member to find the number of bytes read.
          DataBuf::size_ will be 0 on failure.
   */
  DataBuf read(size_t rcount) override;
  /*!
    @brief Read data from the memory blocks. Reading starts at the current
        IO position and the position is advanced by the number of
        bytes read.
        If the memory blocks are not populated (!= bMemory), it will connect to server
        and populate the data to memory blocks.
    @param buf Pointer to a block of memory into which the read data
        is stored. The memory block must be at least \em rcount bytes
        long.
    @param rcount Maximum number of bytes to read. Fewer bytes may be
        read if \em rcount bytes are not available.
    @return Number of bytes read from the memory block successfully;<BR>
           0 if failure;
   */
  size_t read(byte* buf, size_t rcount) override;
  /*!
    @brief Read one byte from the memory blocks. The IO position is
        advanced by one byte.
        If the memory block is not populated (!= bMemory), it will connect to server
        and populate the data to the memory block.
    @return The byte read from the memory block if successful;<BR>
           EOF if failure;
   */
  int getb() override;
  /*!
    @brief Remove the contents of the file and then transfer data from
        the \em src BasicIo object into the empty file.

    The write access is done in an efficient way. It only sends the range of different
    bytes between the current data and BasicIo instance to the remote machine.

    @param src Reference to another BasicIo instance. The entire contents
        of src are transferred to this object. The \em src object is
        invalidated by the method.
    @throw Error In case of failure

    @note The write access is only supported by http, https, ssh.
   */
  void transfer(BasicIo& src) override;

  int seek(int64_t offset, Position pos) override;

  /*!
    @brief Not support
    @return NULL
   */
  byte* mmap(bool /*isWriteable*/ = false) override;
  /*!
    @brief Not support
    @return 0
   */
  int munmap() override;
  //@}
  //! @name Accessors
  //@{
  /*!
    @brief Get the current IO position.
    @return Offset from the start of the memory block
   */
  [[nodiscard]] size_t tell() const override;
  /*!
    @brief Get the current memory buffer size in bytes.
    @return Size of the in memory data in bytes;<BR>
           -1 if failure;
   */
  [[nodiscard]] size_t size() const override;
  //! Returns true if the memory area is allocated.
  [[nodiscard]] bool isopen() const override;
  //! Always returns 0
  [[nodiscard]] int error() const override;
  //! Returns true if the IO position has reached the end, otherwise false.
  [[nodiscard]] bool eof() const override;
  //! Returns the URL of the file.
  [[nodiscard]] const std::string& path() const noexcept override;

  /*!
    @brief Mark all the bNone blocks to bKnow. This avoids allocating memory
      for parts of the file that contain image-date (non-metadata/pixel data)

    @note This method should be only called after the concerned data (metadata)
          are all downloaded from the remote file to memory.
   */
  void populateFakeData() override;

  //@}

 protected:
  // Pimpl idiom
  class Impl;
  //! Pointer to implementation
  std::unique_ptr<Impl> p_;
};  // class RemoteIo

/*!
    @brief Provides the http read/write access for the RemoteIo.
*/
class EXIV2API HttpIo : public RemoteIo {
 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor that accepts the http URL on which IO will be
        performed. The constructor does not open the file, and
        therefore never fails.
    @param url The full path of url
    @param blockSize the size of the memory block. The file content is
          divided into the memory blocks. These blocks are populated
          on demand from the server, so it avoids copying the complete file.
   */
  explicit HttpIo(const std::string& url, size_t blockSize = 1024);

 private:
  // Pimpl idiom
  class HttpImpl;
};

#ifdef EXV_USE_CURL
/*!
    @brief Provides the http, https read/write access and ftp read access for the RemoteIo.
        This class is based on libcurl.
*/
class EXIV2API CurlIo : public RemoteIo {
 public:
  //! @name Creators
  //@{
  /*!
    @brief Constructor that accepts the URL on which IO will be
        performed.
    @param url The full path of url
    @param blockSize the size of the memory block. The file content is
          divided into the memory blocks. These blocks are populated
          on demand from the server, so it avoids copying the complete file.
    @throw Error if it is unable to init curl pointer.
   */
  explicit CurlIo(const std::string& url, size_t blockSize = 0);

  /*!
    @brief Write access is only available for some protocols. This method
          will call RemoteIo::write(const byte* data, long wcount) if the write
          access is available for the protocol. Otherwise, it throws the Error.
   */
  size_t write(const byte* data, size_t wcount) override;
  /*!
    @brief Write access is only available for some protocols. This method
          will call RemoteIo::write(BasicIo& src) if the write access is available
          for the protocol. Otherwise, it throws the Error.
   */
  size_t write(BasicIo& src) override;

 protected:
  // Pimpl idiom
  class CurlImpl;
};
#endif

// *****************************************************************************
// template, inline and free functions

/*!
  @brief Read file \em path into a DataBuf, which is returned.
  @return Buffer containing the file.
  @throw Error In case of failure.
 */
EXIV2API DataBuf readFile(const std::string& path);
/*!
  @brief Write DataBuf \em buf to file \em path.
  @return Return the number of bytes written.
  @throw Error In case of failure.
 */
EXIV2API size_t writeFile(const DataBuf& buf, const std::string& path);
#ifdef EXV_USE_CURL
/*!
  @brief The callback function is called by libcurl to write the data
*/
EXIV2API size_t curlWriter(char* data, size_t size, size_t nmemb, std::string* writerData);
#endif
}  // namespace Exiv2
#endif  // #ifndef BASICIO_HPP_
