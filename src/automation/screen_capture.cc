#include "screen_capture.h"

#include <Windows.Graphics.Capture.Interop.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3d11.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.System.h>

#include <atomic>
#include <mutex>

struct __declspec(uuid(
    "A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1")) IDirect3DDxgiInterfaceAccess
    : ::IUnknown {
  virtual HRESULT __stdcall GetInterface(GUID const &id, void **object) = 0;
};

extern "C" {
HRESULT __stdcall CreateDirect3D11DeviceFromDXGIDevice(
    ::IDXGIDevice *dxgiDevice, ::IInspectable **graphicsDevice);
}

namespace dfg {
static auto create_d3d11_device() {
  auto create_d3d_device = [](D3D_DRIVER_TYPE const type,
                              winrt::com_ptr<ID3D11Device> &device) {
    WINRT_ASSERT(!device);
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    return ::D3D11CreateDevice(nullptr, type, nullptr, flags, nullptr, 0,
                               D3D11_SDK_VERSION, device.put(), nullptr,
                               nullptr);
  };
  auto create_d3d_device_wrapper = [&create_d3d_device]() {
    winrt::com_ptr<ID3D11Device> device;
    HRESULT hr = create_d3d_device(D3D_DRIVER_TYPE_HARDWARE, device);
    if (DXGI_ERROR_UNSUPPORTED == hr) {
      hr = create_d3d_device(D3D_DRIVER_TYPE_WARP, device);
    }
    winrt::check_hresult(hr);
    return device;
  };

  auto d3d_device = create_d3d_device_wrapper();
  auto dxgi_device = d3d_device.as<IDXGIDevice>();

  winrt::com_ptr<::IInspectable> d3d11_device;
  winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(
      dxgi_device.get(), d3d11_device.put()));
  return d3d11_device
      .as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
}

template <typename T>
static auto
get_dxgi_interface(winrt::Windows::Foundation::IInspectable const &object) {
  auto access = object.as<IDirect3DDxgiInterfaceAccess>();
  winrt::com_ptr<T> result;
  winrt::check_hresult(
      access->GetInterface(winrt::guid_of<T>(), result.put_void()));
  return result;
}

static auto create_capture_item_for_window(HWND hwnd) {
  auto activation_factory = winrt::get_activation_factory<
      winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
  auto interop_factory = activation_factory.as<IGraphicsCaptureItemInterop>();
  winrt::Windows::Graphics::Capture::GraphicsCaptureItem item = {nullptr};
  interop_factory->CreateForWindow(
      hwnd,
      winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
      reinterpret_cast<void **>(winrt::put_abi(item)));
  return item;
}

static auto create_capture_item_for_monitor(HMONITOR hmonitor) {
  auto activation_factory = winrt::get_activation_factory<
      winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
  auto interop_factory = activation_factory.as<IGraphicsCaptureItemInterop>();
  winrt::Windows::Graphics::Capture::GraphicsCaptureItem item = {nullptr};
  interop_factory->CreateForMonitor(
      hmonitor,
      winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
      reinterpret_cast<void **>(winrt::put_abi(item)));
  return item;
}

cv::Mat capture_internal(
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem const &item) {
  if (!item) {
    return cv::Mat();
  }

  static winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice
      d3d11_direct_device = create_d3d11_device();
  static winrt::com_ptr<ID3D11Device> d3d11_device =
      get_dxgi_interface<ID3D11Device>(d3d11_direct_device);
  winrt::com_ptr<ID3D11DeviceContext> d3d11_device_context;
  d3d11_device->GetImmediateContext(d3d11_device_context.put());

  auto frame_pool =
      winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::Create(
          d3d11_direct_device,
          winrt::Windows::Graphics::DirectX::DirectXPixelFormat::
              B8G8R8A8UIntNormalized,
          1, item.Size());

  auto session = frame_pool.CreateCaptureSession(item);
  session.StartCapture();

  winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame frame = nullptr;
  for (int i = 0; i < 10; ++i) {
    frame = frame_pool.TryGetNextFrame();
    if (frame) {
      break;
    }
    Sleep(10);
  }

  if (!frame) {
    session.Close();
    frame_pool.Close();
    return cv::Mat();
  }

  auto frame_captured_texture =
      get_dxgi_interface<ID3D11Texture2D>(frame.Surface());

  D3D11_TEXTURE2D_DESC desc;
  frame_captured_texture->GetDesc(&desc);

  D3D11_TEXTURE2D_DESC map_desc = {};
  map_desc.Width = desc.Width;
  map_desc.Height = desc.Height;
  map_desc.MipLevels = 1;
  map_desc.ArraySize = 1;
  map_desc.Format = desc.Format;
  map_desc.SampleDesc.Count = 1;
  map_desc.Usage = D3D11_USAGE_STAGING;
  map_desc.BindFlags = 0;
  map_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  map_desc.MiscFlags = 0;

  winrt::com_ptr<ID3D11Texture2D> mapped_texture;
  winrt::check_hresult(
      d3d11_device->CreateTexture2D(&map_desc, nullptr, mapped_texture.put()));

  d3d11_device_context->CopyResource(mapped_texture.get(),
                                     frame_captured_texture.get());

  D3D11_MAPPED_SUBRESOURCE map_result;
  winrt::check_hresult(d3d11_device_context->Map(
      mapped_texture.get(), 0, D3D11_MAP_READ, 0, &map_result));

  cv::Mat image(desc.Height, desc.Width, CV_8UC4);

  for (UINT y = 0; y < desc.Height; ++y) {
    memcpy(image.ptr(y),
           reinterpret_cast<BYTE *>(map_result.pData) + y * map_result.RowPitch,
           desc.Width * 4);
  }

  d3d11_device_context->Unmap(mapped_texture.get(), 0);

  session.Close();
  frame_pool.Close();

  return image;
}

cv::Mat ScreenCapture::capture_screen() {

  POINT pt = {0, 0};
  HMONITOR hmonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
  return capture_internal(create_capture_item_for_monitor(hmonitor));
}

cv::Mat ScreenCapture::capture_window(HWND hwnd) {
  return capture_internal(create_capture_item_for_window(hwnd));
}

} // namespace dfg