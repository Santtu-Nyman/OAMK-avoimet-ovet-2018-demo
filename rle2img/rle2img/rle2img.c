// OAMK Avoimet ovet 2018 demo by Santtu Nyman

#define _CRT_SECURE_NO_WARNINGS
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

int extract_rle(size_t data_size, const void* data, size_t* rle_length, void** rle_data);

int decompress_rle(size_t rle_length, uint8_t* rle, size_t* data_length, uint8_t** data);

void iodct8x8(const float* in, float* out);

int load_file(const char* file_name, size_t* file_size, void** file_data);

int store_file(const char* file_name, size_t file_size, const void* file_data);

int show_image(size_t width, size_t height, const uint8_t* pixels);

int main(int argc, char** argv)
{
	int error;
	if (argc < 2)
	{
		error = EINVAL;
		return error;
	}

	size_t input_file_size;
	void* input_file_data;
	error = load_file(argv[1], &input_file_size, &input_file_data);
	if (error)
		return error;

	size_t rle_length;
	uint8_t* rle_data;
	error = extract_rle(input_file_size, input_file_data, &rle_length, &rle_data);
	free(input_file_data);
	if (error)
		return error;

	size_t sub_matrix_data_length;
	int8_t* sub_matrix_data;
	error = decompress_rle(rle_length, rle_data, &sub_matrix_data_length, (uint8_t**)&sub_matrix_data);
	free(rle_data);
	if (error)
		return error;

	if (sub_matrix_data_length < 8 * 8 * 8 * 8)
	{
		int8_t* sub_matrix_realloc = (int8_t*)realloc(sub_matrix_data, 8 * 8 * 8 * 8);
		if (!sub_matrix_realloc)
		{
			error = ENOMEM;
			free(sub_matrix_data);
			return error;
		}
		sub_matrix_data = sub_matrix_realloc;
		memset(sub_matrix_data + sub_matrix_data_length, 0, 8 * 8 * 8 * 8 - sub_matrix_data_length);
	}

	float* sub_matrices = (float*)malloc(2 * 8 * 8 * 8 * 8 * sizeof(float));
	float* transformed_sub_matrices = sub_matrices + 8 * 8 * 8 * 8;
	if (!sub_matrices)
	{
		error = ENOMEM;
		free(sub_matrix_data);
		return error;
	}

	for (int m = 0; m != 8 * 8; ++m)
		for (int i = 0; i != 8; ++i)
			for (int j = 0; j != 8; ++j)
				sub_matrices[64 * m + 8 * i + j] = (float)sub_matrix_data[64 * m + i + 8 * j];
	free(sub_matrix_data);

	const float quantization_matrix[8 * 8] = {
		16.0f, 11.0f, 10.0f, 16.0f, 24.0f, 40.0f, 51.0f, 61.0f,
		12.0f, 12.0f, 14.0f, 19.0f, 26.0f, 58.0f, 60.0f, 55.0f,
		14.0f, 13.0f, 16.0f, 24.0f, 40.0f, 57.0f, 69.0f, 56.0f,
		14.0f, 17.0f, 22.0f, 29.0f, 51.0f, 87.0f, 80.0f, 62.0f,
		18.0f, 22.0f, 37.0f, 56.0f, 68.0f, 109.0f, 103.0f, 77.0f,
		24.0f, 35.0f, 55.0f, 64.0f, 81.0f, 104.0f, 113.0f, 92.0f,
		49.0f, 64.0f, 78.0f, 87.0f, 103.0f, 121.0f, 120.0f, 101.0f,
		72.0f, 92.0f, 95.0f, 98.0f, 112.0f, 100.0f, 103.0f, 99.0f };
	for (int m = 0; m != 8 * 8; ++m)
	{
		for (int i = 0; i != 8; ++i)
			for (int j = 0; j != 8; ++j)
				sub_matrices[8 * 8 * m + 8 * i + j] *= quantization_matrix[8 * i + j];
		iodct8x8(sub_matrices + 8 * 8 * m, transformed_sub_matrices + 8 * 8 * m);
		for (int i = 0; i != 8; ++i)
			for (int j = 0; j != 8; ++j)
				transformed_sub_matrices[8 * 8 * m + 8 * i + j] += 128.0f;
	}

	/*
	for (int m = 0; m != 64; ++m)
	{
		printf("\n");
		for (int i = 0; i != 8; ++i)
		{
			for (int j = 0; j != 8; ++j)
				printf("%f ", transformed_sub_matrices[64 * m + 8 * i + j]);
			printf("\n");
		}
	}
	*/

	const int scale = 8;
	uint8_t* image = (uint8_t*)malloc(64 * scale * 64 * scale * 4);

	if (!image)
	{
		error = ENOMEM;
		free(sub_matrices);
		return error;
	}

	for (int m = 0; m != 8 * 8; ++m)
	{
		int o = ((m / 8) * 8 * scale * 64 * scale) + ((m % 8) * 8 * scale);
		for (int i = 0; i != 8; ++i)
			for (int j = 0; j != 8; ++j)
			{
				float intensity = transformed_sub_matrices[8 * 8 * m + 8 * i + j];
				uint8_t color = intensity < 0.0f ? 0 : (uint8_t)intensity;
				for (int h = 0; h != scale; ++h)
					for (int w = 0; w != scale; ++w)
					{
						int y = (scale * 64 - 1) - ((o + 64 * scale * scale * i + j * scale + h * 64 * scale + w) / (64 * scale));
						int x = (o + 64 * scale * scale * i + j * scale + h * 64 * scale + w) % (64 * scale);
						uint8_t* pixel = image + (scale * 64 * y + x) * 4;
						pixel[0] = color;
						pixel[1] = color;
						pixel[2] = color;
					}

			}
	}
	free(sub_matrices);

	show_image(64 * scale, 64 * scale, image);
	free(image);
	return 0;
}

int extract_rle(size_t data_size, const void* data, size_t* rle_length, void** rle_data)
{
	uintptr_t begin = (uintptr_t)data;
	uintptr_t data_end = (uintptr_t)data + data_size;
	while (data_end - begin > 1 && (*(const uint8_t*)begin || *(const uint8_t*)(begin + 1)))
		++begin;
	if (data_end - begin < 2 || *(const uint8_t*)begin || *(const uint8_t*)(begin + 1))
		return ENODATA;
	begin += 2;
	data_end -= (data_end - begin) & 1;
	uintptr_t end = (uintptr_t)begin;
	while (end != data_end && (*(const uint8_t*)end || *(const uint8_t*)(end + 1)))
		end += 2;
	if (end == data_end)
		return ENODATA;
	size_t size = end - begin;
	void* rle = malloc(size);
	if (!rle)
		return ENOMEM;
	memcpy(rle, (const void*)begin, size);
	*rle_length = size >> 1;
	*rle_data = rle;
	return 0;
}

int decompress_rle(size_t rle_length, uint8_t* rle, size_t* data_length, uint8_t** data)
{
	size_t l = 0;
	for (size_t i = 0; i != rle_length; ++i)
		l += (size_t)rle[2 * i + 1];
	uint8_t* d = (uint8_t*)malloc(l);
	if (!d)
		return ENOMEM;
	for (size_t i = 0, w = 0, t; i != rle_length; ++i)
	{
		t = w + (size_t)rle[2 * i + 1];
		uint8_t b = rle[2 * i];
		while (w != t)
			d[w++] = b;
	}
	*data_length = l;
	*data = d;
	return 0;
}

void iodct8x8(const float* in, float* out)
{
	const float pii16 = 0.19634954084f;// pi * (1 / 16)
	const float isqr2 = 0.70710678118f;// 1 / sqrt(2)
	for (int i = 0; i != 8; ++i)
		for (int j = 0; j != 8; ++j)
		{
			float sum = 0.0f;
			for (int u = 0; u != 8; ++u)
			{
				float Cu = u ? 1.0f : isqr2;
				for (int v = 0; v != 8; ++v)
				{
					float Cv = v ? 1.0f : isqr2;
					float idct = Cu * Cv * in[8 * u + v] * cosf((float)(2 * i + 1) * u * pii16) * cosf((float)(2 * j + 1) * v * pii16);
					sum += idct;
				}
			}
			out[8 * i + j] = 0.25f * sum;
		}
}

int load_file(const char* file_name, size_t* file_size, void** file_data)
{
	int error;
	FILE* file = fopen(file_name, "rb");
	if (!file)
	{
		error = errno;
		return error;
	}
	if (fseek(file, 0, SEEK_END))
	{
		error = errno;
		fclose(file);
		return error;
	}
	long end = ftell(file);
	if (end == EOF)
	{
		error = errno;
		fclose(file);
		return error;
	}
	if (fseek(file, 0, SEEK_SET))
	{
		error = errno;
		fclose(file);
		return error;
	}
	if ((sizeof(size_t) < sizeof(long)) && (end > (long)((size_t)~0)))
	{
		error = EFBIG;
		fclose(file);
		return error;
	}
	size_t size = (size_t)end;
	void* data = malloc(size);
	if (!data)
	{
		error = ENOMEM;
		fclose(file);
		return error;
	}
	for (size_t read = 0, result; read != size; read += result)
	{
		result = fread((void*)((uintptr_t)data + read), 1, size - read, file);
		if (!result)
		{
			error = ferror(file);
			free(data);
			fclose(file);
			return error;
		}
	}
	fclose(file);
	*file_size = size;
	*file_data = data;
	return 0;
}

int store_file(const char* file_name, size_t file_size, const void* file_data)
{
	int error;
	size_t file_name_length = strlen(file_name);
	char* temporal_file_name = (char*)malloc(file_name_length + 16);
	if (!temporal_file_name)
	{
		error = ENOMEM;
		return error;
	}
	memcpy(temporal_file_name, file_name, file_name_length);
	int temporal_file_name_index = 0;
	time_t current_time;
	struct tm* current_date = time(&current_time) != -1 ? localtime(&current_time) : 0;
	if (current_date)
	{
		temporal_file_name[file_name_length] = '.';
		for (int i = 0, v = current_date->tm_hour; i != 2; ++i, v /= 10)
			temporal_file_name[file_name_length + 1 + (1 - i)] = '0' + (char)(v % 10);
		for (int i = 0, v = current_date->tm_min; i != 2; ++i, v /= 10)
			temporal_file_name[file_name_length + 3 + (1 - i)] = '0' + (char)(v % 10);
		for (int i = 0, v = current_date->tm_sec; i != 2; ++i, v /= 10)
			temporal_file_name[file_name_length + 5 + (1 - i)] = '0' + (char)(v % 10);
		memcpy(temporal_file_name + file_name_length + 7, "0000.tmp", 9);
	}
	else
		memcpy(temporal_file_name + file_name_length, ".0000000000.tmp", 16);
	FILE* file = 0;
	while (!file)
	{
		file = fopen(temporal_file_name, "rb");
		if (!file)
		{
			file = fopen(temporal_file_name, "wb");
			if (!file)
			{
				error = errno;
				free(temporal_file_name);
				return error;
			}
		}
		else
		{
			fclose(file);
			file = 0;
			if (temporal_file_name_index == 9999)
			{
				error = EEXIST;
				free(temporal_file_name);
				return error;
			}
			for (int i = 0, v = temporal_file_name_index++; i != 4; ++i, v /= 10)
				temporal_file_name[file_name_length + 7 + (3 - i)] = '0' + (char)(v % 10);
		}
	}
	for (size_t written = 0, write_result; written != file_size; written += write_result)
	{
		write_result = fwrite((const void*)((uintptr_t)file_data + written), 1, file_size - written, file);
		if (!write_result)
		{
			error = ferror(file);
			fclose(file);
			remove(temporal_file_name);
			free(temporal_file_name);
			return error;
		}
	}
	if (fflush(file))
	{
		error = ferror(file);
		fclose(file);
		remove(temporal_file_name);
		free(temporal_file_name);
		return error;
	}
	fclose(file);
	if (rename(temporal_file_name, file_name))
	{
		char* backup_file_name = (char*)malloc(file_name_length + 16);
		if (!backup_file_name)
		{
			remove(temporal_file_name);
			free(temporal_file_name);
			error = ENOMEM;
			return error;
		}
		memcpy(backup_file_name, temporal_file_name, file_name_length + 16);
		for (int backup_file_name_index = temporal_file_name_index + 1; backup_file_name_index < 10000; ++backup_file_name_index)
		{
			for (int i = 0, v = backup_file_name_index; i != 4; ++i, v /= 10)
				backup_file_name[file_name_length + 7 + (3 - i)] = '0' + (char)(v % 10);
			file = fopen(backup_file_name, "rb");
			if (!file)
			{
				if (rename(file_name, backup_file_name))
				{
					error = errno;
					remove(temporal_file_name);
					free(backup_file_name);
					free(temporal_file_name);
					return error;
				}
				if (rename(temporal_file_name, file_name))
				{
					error = errno;
					rename(backup_file_name, file_name);
					remove(temporal_file_name);
					free(backup_file_name);
					free(temporal_file_name);
					return error;
				}
				remove(backup_file_name);
				free(backup_file_name);
				free(temporal_file_name);
				return 0;
			}
			else
				fclose(file);
		}
		error = EEXIST;
		remove(temporal_file_name);
		free(backup_file_name);
		free(temporal_file_name);
		return error;
	}
	free(temporal_file_name);
	return 0;
}

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

LRESULT CALLBACK show_image_window_procedure(HWND window, UINT message, WPARAM w_parameter, LPARAM l_parameter)
{
	BITMAPINFOHEADER* image = (BITMAPINFOHEADER*)GetWindowLongPtrW(window, GWLP_USERDATA);
	switch (message)
	{
		case WM_PAINT:
		{
			PAINTSTRUCT paint;
			HDC context = BeginPaint(window, &paint);
			StretchDIBits(context,
				paint.rcPaint.left,
				paint.rcPaint.top,
				paint.rcPaint.right - paint.rcPaint.left,
				paint.rcPaint.bottom - paint.rcPaint.top,
				paint.rcPaint.left,
				image->biHeight - paint.rcPaint.bottom,
				paint.rcPaint.right - paint.rcPaint.left,
				paint.rcPaint.bottom - paint.rcPaint.top,
				(const void*)((UINT_PTR)image + sizeof(BITMAPINFOHEADER)),
				(const BITMAPINFO*)image, DIB_RGB_COLORS, SRCCOPY);
			EndPaint(window, &paint);
			return 0;
		}
		case WM_SIZE:
		{
			if (w_parameter != SIZE_MINIMIZED)
			{
				RECT window_rectangle;
				if (GetClientRect(window, &window_rectangle) && (window_rectangle.right - window_rectangle.left != image->biWidth || window_rectangle.bottom - window_rectangle.top != image->biHeight))
				{
					window_rectangle.left = 0;
					window_rectangle.top = 0;
					window_rectangle.right = image->biWidth;
					window_rectangle.bottom = image->biHeight;
					if (AdjustWindowRectEx(&window_rectangle, (DWORD)GetWindowLongPtrW(window, GWL_STYLE), FALSE, (DWORD)GetWindowLongPtrW(window, GWL_EXSTYLE)))
						SetWindowPos(window, HWND_TOP, 0, 0, (int)(window_rectangle.right - window_rectangle.left), (int)(window_rectangle.bottom - window_rectangle.top), SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
				}
			}
			return 0;
		}
		case WM_ERASEBKGND:
			return 1;
		case WM_TIMER:
		case WM_KEYDOWN:
			return 0;
		case WM_CLOSE:
		case WM_DESTROY:
		case WM_QUIT:
		{
			SetWindowLongPtrW(window, GWLP_USERDATA, 0);
			SetWindowPos(window, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER);
			PostQuitMessage(0);
			return 0;
		}
		case WM_CREATE:
		{
			image = (BITMAPINFOHEADER*)((CREATESTRUCT*)l_parameter)->lpCreateParams;
			SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)image);
			SetWindowPos(window, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOSIZE | SWP_NOZORDER);
			if (GetWindowLongPtrW(window, GWLP_USERDATA) != (LONG_PTR)image)
				return (LRESULT)-1;
			InvalidateRect(window, 0, FALSE);
			return (LRESULT)0;
		}
		default:
			return DefWindowProcW(window, message, w_parameter, l_parameter);
	}
}

int show_image(size_t width, size_t height, const uint8_t* pixels)
{
	HANDLE heap = GetProcessHeap();
	if (!heap)
		return -1;
	BITMAPINFOHEADER* image = HeapAlloc(heap, 0, sizeof(BITMAPINFOHEADER) + height * width * 4);
	if (!image)
		return -1;

	image->biSize = sizeof(BITMAPINFOHEADER);
	image->biWidth = (LONG)width;
	image->biHeight = (LONG)height;
	image->biPlanes = 1;
	image->biBitCount = 32;
	image->biCompression = BI_RGB;
	image->biSizeImage = 0;
	image->biXPelsPerMeter = 0;
	image->biYPelsPerMeter = 0;
	image->biClrUsed = 0;
	image->biClrImportant = 0;
	CopyMemory((void*)((UINT_PTR)image + sizeof(BITMAPINFOHEADER)), pixels, height * width * 4);

	WNDCLASSEXW window_class_info;
	window_class_info.cbSize = sizeof(WNDCLASSEXW);
	window_class_info.style = CS_HREDRAW | CS_VREDRAW;
	window_class_info.lpfnWndProc = show_image_window_procedure;
	window_class_info.cbClsExtra = 0;
	window_class_info.cbWndExtra = 0;
	window_class_info.hInstance = GetModuleHandleW(0);
	window_class_info.hIcon = 0;// LoadIconW(0, (const WCHAR*)IDI_APPLICATION);
	window_class_info.hCursor = LoadCursorW(0, (const WCHAR*)IDC_ARROW);
	window_class_info.hbrBackground = 0;
	window_class_info.lpszMenuName = 0;
	window_class_info.lpszClassName = L"OAMK_AVOIMET_OVET_2018_DEMO";
	window_class_info.hIconSm = 0;// (HICON)LoadImageW(window_class_info.hInstance, MAKEINTRESOURCEW(5), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	if (!RegisterClassExW(&window_class_info))
	{
		HeapFree(heap, 0, image);
		return -1;
	}

	RECT window_rectangle = { 0, 0, (LONG)width, (LONG)height };
	if (!AdjustWindowRectEx(&window_rectangle, (WS_OVERLAPPEDWINDOW | WS_VISIBLE) & ~WS_THICKFRAME, FALSE, WS_EX_APPWINDOW | WS_EX_DLGMODALFRAME))
	{
		UnregisterClassW(window_class_info.lpszClassName, window_class_info.hInstance);
		HeapFree(heap, 0, image);
		return -1;
	}

	HWND window = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_DLGMODALFRAME,
		window_class_info.lpszClassName,
		L"OAMK avoimet ovet 2018 demo",
		(WS_OVERLAPPEDWINDOW | WS_VISIBLE) & ~WS_THICKFRAME,
		CW_USEDEFAULT, CW_USEDEFAULT,
		window_rectangle.right - window_rectangle.left,
		window_rectangle.bottom - window_rectangle.top,
		0, 0, window_class_info.hInstance,
		(LPVOID)image);

	if (!window)
	{
		UnregisterClassW(window_class_info.lpszClassName, window_class_info.hInstance);
		HeapFree(heap, 0, image);
		return -1;
	}

	MSG window_message;
	for (BOOL loop = GetMessageW(&window_message, window, 0, 0); loop && loop != (BOOL)-1 && (BITMAPINFOHEADER*)GetWindowLongPtrW(window, GWLP_USERDATA) == image; loop = GetMessageW(&window_message, window, 0, 0))
	{
		TranslateMessage(&window_message);
		DispatchMessageW(&window_message);
	}

	DestroyWindow(window);
	UnregisterClassW(window_class_info.lpszClassName, window_class_info.hInstance);
	HeapFree(heap, 0, image);
	return 0;
}