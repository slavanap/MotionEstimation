Задание: 3 / Алгоритм компенсации движения
	[+] основная часть (16x16 blocks, halfpixel)
	[+] дополнительная часть (8x8 blocks)

Система: Visual Studio 2008
ОС: Windows XP Professional
Аппаратура: CPU 1,7GHz, RAM 512MB, Video - (internal) Intel Chipset Family 

Комментарии: 
	Для того, чтобы отключить использование блоков 8х8, необходимо
закомментировать строчку в начале файла MEFunctions.cpp :
	#define ENABLE_SPLIT
