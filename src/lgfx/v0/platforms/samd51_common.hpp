#if defined (__SAMD51__)

#ifndef LGFX_SAMD51_COMMON_HPP_
#define LGFX_SAMD51_COMMON_HPP_

#include "../lgfx_common.hpp"

#include <malloc.h>

#if defined ( ARDUINO )

 #include <sam.h>
 #include <delay.h>
 #include <Arduino.h>

#else

 // This has been defined once to prevent the dependency graph from malfunctioning when using platform IO with ESP32.
 #define INCLUDE_FREERTOS_PATH <FreeRTOS.h> 
 #include INCLUDE_FREERTOS_PATH
 #undef INCLUDE_FREERTOS_PATH
 #include <task.h>
 #include <config/default/system/fs/sys_fs.h>
 #include "samd51_arduino_compat.hpp"

 #undef PORT_PINCFG_PULLEN
 #undef PORT_PINCFG_PULLEN_Pos
 #undef PORT_PINCFG_INEN
 #undef PORT_PINCFG_INEN_Pos

 #define _Ul(n) (static_cast<std::uint32_t>((n)))
 #define PORT_PINCFG_INEN_Pos        1            /**< \brief (PORT_PINCFG) Input Enable */
 #define PORT_PINCFG_INEN            (_Ul(0x1) << PORT_PINCFG_INEN_Pos)
 #define PORT_PINCFG_PULLEN_Pos      2            /**< \brief (PORT_PINCFG) Pull Enable */
 #define PORT_PINCFG_PULLEN          (_Ul(0x1) << PORT_PINCFG_PULLEN_Pos)

#endif
namespace lgfx
{
 inline namespace v0
 {
//----------------------------------------------------------------------------

  namespace samd51
  {
    static constexpr int PORT_SHIFT = 8;
    enum pin_port
    {
      PORT_A =  0 << PORT_SHIFT,
      PORT_B =  1 << PORT_SHIFT,
      PORT_C =  2 << PORT_SHIFT,
      PORT_D =  3 << PORT_SHIFT,
      PORT_E =  4 << PORT_SHIFT,
      PORT_F =  5 << PORT_SHIFT,
      PORT_G =  6 << PORT_SHIFT,
      PORT_H =  7 << PORT_SHIFT,
    };
  }

#if defined ( ARDUINO )

  __attribute__ ((unused))
  static inline unsigned long millis(void)
  {
    return ::millis();
  }
  __attribute__ ((unused))
  static inline unsigned long micros(void)
  {
    return ::micros();
  }
  __attribute__ ((unused))
  static inline void delay(unsigned long milliseconds)
  {
    ::delay(milliseconds);
  }
  __attribute__ ((unused))
  static void delayMicroseconds(unsigned int us)
  {
    ::delayMicroseconds(us);
  }

#else

  static inline void delay(std::size_t milliseconds)
  {
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
  }

  static void delayMicroseconds(unsigned int us)
  {
    uint32_t start, elapsed;
    uint32_t count;

    if (us == 0)
      return;

    count = us * (VARIANT_MCK / 1000000) - 20;  // convert us to cycles.
    start = DWT->CYCCNT;  //CYCCNT is 32bits, takes 37s or so to wrap.
    while (1) {
      elapsed = DWT->CYCCNT - start;
      if (elapsed >= count)
        return;
    }
  }

#endif

  static inline void* heap_alloc(      size_t length) { return malloc(length); }
  static inline void* heap_alloc_psram(size_t length) { return malloc(length); }
  static inline void* heap_alloc_dma(  size_t length) { return memalign(16, length); }
  static inline void heap_free(void* buf) { free(buf); }

  static inline void gpio_hi(std::uint32_t pin) { PORT->Group[pin>>8].OUTSET.reg = (1ul << (pin & 0xFF)); }
  static inline void gpio_lo(std::uint32_t pin) { PORT->Group[pin>>8].OUTCLR.reg = (1ul << (pin & 0xFF)); }
  static inline bool gpio_in(std::uint32_t pin) { return PORT->Group[pin>>8].IN.reg & (1ul << (pin & 0xFF)); }


//  static void initPWM(std::uint32_t pin, std::uint32_t pwm_ch, std::uint8_t duty = 128) 
  static inline void initPWM(std::uint32_t , std::uint32_t , std::uint32_t , std::uint8_t = 0) {
// unimplemented 
  }

//  static void setPWMDuty(std::uint32_t pwm_ch, std::uint8_t duty = 128) 
  static inline void setPWMDuty(std::uint32_t , std::uint8_t = 0 ) {
// unimplemented 
  }

  enum pin_mode_t
  { output
  , input
  , input_pullup
  , input_pulldown
  };

  void lgfxPinMode(std::uint32_t pin, pin_mode_t mode);

//----------------------------------------------------------------------------
  struct FileWrapper : public DataWrapper
  {
    FileWrapper() : DataWrapper() { need_transaction = true; }

#if defined (ARDUINO) && defined (__SEEED_FS__)

    fs::File _file;
    fs::File *_fp;

    fs::FS *_fs = nullptr;
    void setFS(fs::FS& fs) {
      _fs = &fs;
      need_transaction = false;
    }
    FileWrapper(fs::FS& fs) : DataWrapper(), _fp(nullptr) { setFS(fs); }
    FileWrapper(fs::FS& fs, fs::File* fp) : DataWrapper(), _fp(fp) { setFS(fs); }

    bool open(fs::FS& fs, const char* path, const char* mode) {
      setFS(fs);
      return open(path, mode);
    }

    bool open(const char* path, const char* mode) { 
      fs::File file = _fs->open(path, mode);
      // この邪悪なmemcpyは、Seeed_FSのFile実装が所有権moveを提供してくれないのにデストラクタでcloseを呼ぶ実装になっているため、
      // 正攻法ではFileをクラスメンバに保持できない状況を打開すべく応急処置的に実装したものです。
      memcpy(&_file, &file, sizeof(fs::File));
      // memsetにより一時変数の中身を吹っ飛ばし、デストラクタによるcloseを予防します。
      memset(&file, 0, sizeof(fs::File));
      _fp = &_file;
      return _file;
    }

    int read(std::uint8_t *buf, std::uint32_t len) override { return _fp->read(buf, len); }
    void skip(std::int32_t offset) override { seek(offset, SeekCur); }
    bool seek(std::uint32_t offset) override { return seek(offset, SeekSet); }
    bool seek(std::uint32_t offset, SeekMode mode) { return _fp->seek(offset, mode); }
    void close() override { _fp->close(); }
    std::int32_t tell(void) override { return _fp->position(); }

#elif __SAMD51_HARMONY__

    SYS_FS_HANDLE handle = SYS_FS_HANDLE_INVALID;

    bool open(const char* path, const char* mode) 
    { 
      SYS_FS_FILE_OPEN_ATTRIBUTES attributes = SYS_FS_FILE_OPEN_ATTRIBUTES::SYS_FS_FILE_OPEN_READ;
      switch(mode[0])
      {
        case 'r': attributes = SYS_FS_FILE_OPEN_ATTRIBUTES::SYS_FS_FILE_OPEN_READ; break;
        case 'w': attributes = SYS_FS_FILE_OPEN_ATTRIBUTES::SYS_FS_FILE_OPEN_WRITE; break;
      }
      this->handle = SYS_FS_FileOpen(path, attributes);
      return this->handle != SYS_FS_HANDLE_INVALID;
    }
    int read(std::uint8_t* buffer, std::uint32_t length) override 
    {
      return SYS_FS_FileRead(this->handle, buffer, length);
    }
    void skip(std::int32_t offset) override 
    { 
      SYS_FS_FileSeek(this->handle, offset, SYS_FS_FILE_SEEK_CONTROL::SYS_FS_SEEK_CUR);
    }
    bool seek(std::uint32_t offset) override 
    {
      return SYS_FS_FileSeek(this->handle, offset, SYS_FS_FILE_SEEK_CONTROL::SYS_FS_SEEK_SET) >= 0;
    }
    bool seek(std::uint32_t offset, SYS_FS_FILE_SEEK_CONTROL mode) 
    {
      return SYS_FS_FileSeek(this->handle, offset, mode) >= 0;
    }
    void close() override 
    {
      if( this->handle != SYS_FS_HANDLE_INVALID ) {
        SYS_FS_FileClose(this->handle);
        this->handle = SYS_FS_HANDLE_INVALID;
      }
    }
    std::int32_t tell(void) override
    {
      return SYS_FS_FileTell(this->handle);
    }

#else  // dummy.

    bool open(const char*, const char*) { return false; }
    int read(std::uint8_t*, std::uint32_t) override { return 0; }
    void skip(std::int32_t) override { }
    bool seek(std::uint32_t) override { return false; }
    bool seek(std::uint32_t, int) { return false; }
    void close() override { }
    std::int32_t tell(void) override { return 0; }

#endif

  };

//----------------------------------------------------------------------------

#if defined (ARDUINO) && defined (Stream_h)

  struct StreamWrapper : public DataWrapper
  {
    void set(Stream* src, std::uint32_t length = ~0u) { _stream = src; _length = length; _index = 0; }

    int read(std::uint8_t *buf, std::uint32_t len) override {
      if (len > _length - _index) { len = _length - _index; }
      _index += len;
      return _stream->readBytes((char*)buf, len);
    }
    void skip(std::int32_t offset) override { if (0 < offset) { char dummy[offset]; _stream->readBytes(dummy, offset); _index += offset; } }
    bool seek(std::uint32_t offset) override { if (offset < _index) { return false; } skip(offset - _index); return true; }
    void close() override { }
    std::int32_t tell(void) override { return _index; }

  private:
    Stream* _stream;
    std::uint32_t _index;
    std::uint32_t _length = 0;

  };

#endif

//----------------------------------------------------------------------------
 }
}

#endif

#endif