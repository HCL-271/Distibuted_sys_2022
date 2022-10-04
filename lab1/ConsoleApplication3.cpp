#include <iostream>
#include <vector>
#include <thread>
#include <string>

using namespace std;


typedef vector<vector<double> > Matrix;

struct input {

	Matrix &First, &Second, &Result;

	size_t left_index, right_index, size;
};

void Many_threads(input in) {

	for (size_t i = in.left_index; i < in.right_index; i++) {

		for (size_t j = 0; j < in.size; j++) {

			for (size_t k = 0; k < in.size; k++) {

				in.Result[i][j] += in.First[i][k] * in.Second[j][k];

			}
		}
	}
}

Matrix Multiply(Matrix& First, Matrix& Second, size_t number_of_threads) {
	size_t rank = First.size();

	/*vector<size_t> index(n_threads + 1, 0);
	index[n_threads] = rank;
	for(int i = 1; i < n_threads; i++) {
		index[i] = index[i - 1] + (rank / n_threads) + ((i - 1) < ((rank % n_threads) - 2));
	}*/

	Matrix transponent(rank, vector<double>(rank, 0));

	for (size_t i = 0; i < rank; i++) {

		for (size_t j = 0; j < rank; j++) {

			transponent[j][i] = Second[i][j];

		}
	}

	Matrix res(rank, vector<double>(rank, 0));
//https://thispointer.com/c11-how-to-create-vector-of-thread-objects/
	std::vector<std::thread> thread;

	for (size_t i = 0; i < number_of_threads; i++) {

		size_t left = i * (rank / number_of_threads);
		size_t right = (i == number_of_threads - 1) ? rank : (i + 1) * (rank / number_of_threads);

		input in = { First, transponent, res, left, right, rank };

		//cout << left << " " << right << endl;

		thread.push_back(std::thread(&Many_threads, in));
	}

	for (size_t i = 0; i < number_of_threads; i++) {

		thread[i].join();
		//Блокирует вызывающий поток до завершения потока, представленного этим экземпляром.
	}

	return res;
}


Matrix MultiplyWithOutAMP(Matrix& matrix_A, Matrix& matrix_B, size_t n_threads) {

	size_t rank = matrix_A.size();

	/*vector<size_t> index(n_threads + 1, 0);
	index[n_threads] = rank;
	for(int i = 1; i < n_threads; i++) {
		index[i] = index[i - 1] + (rank / n_threads) + ((i - 1) < ((rank % n_threads) - 2));
	}*/

	Matrix transponent(rank, vector<double>(rank, 0));
	Matrix res(rank, vector<double>(rank, 0));

	for (int row = 0; row < rank; row++) {
		for (int col = 0; col < rank; col++) {
			// Multiply the row of A by the column of B to get the row, column of product.
			for (int inner = 0; inner < rank-1; inner++) {
				res[row][col] += matrix_A[row][inner] * matrix_B[inner][col];
			}
			//std::cout << answer[row][col] << "  ";
		}
		//std::cout << "\n";
	}

	

	

	return res;
}


int main(int argc, char** argv) {

	for (size_t k = 0; k < 100; k++) {



		size_t n_threads = 6, size = k, average = 3;
		

		// Считывание аргументов
		if (argc > 1) {

			n_threads = stoll(argv[1]);

			if (argc > 2) {

				size = stoll(argv[2]);

				if (argc > 3) {

					average = stoll(argv[3]);
				}
			}
		}

		/*size_t auxiliary = 1;
		while(auxiliary < rank) {
			auxiliary <<= 1;
		}
		rank = auxiliary;*/
		
		Matrix  First(size, vector<double>(size, 1));

		//matrix[rank / 2][rank / 2] = 2;
		Matrix  Res(size, vector<double>(size, 0));

		double time = 0;

		for (size_t i = 0; i < average; i++) {

			auto start = std::chrono::system_clock::now();

			Res = Multiply(First, First, n_threads);
			//Res	 =  MultiplyWithOutAMP(First, First, n_threads);
			auto end = std::chrono::system_clock::now();
			std::chrono::duration<double> wasted = end - start;
			time += wasted.count();
		}
		
		/*for(size_t i = 0; i < rank; i++) {
			for(size_t j = 0; j < rank; j++) {
				cout << matrix_C[i][j] << " ";
			}
			cout << endl;
		}*/

		//std::cout << n_threads << " " << rank << " " << time / average << std::endl;

		
		std::cout << size << " " << time / average << std::endl;
	}
	return EXIT_SUCCESS;
}
