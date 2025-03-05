#include "amx.hpp"
// int64_t looptime = 100000000;

static double get_time(struct timespec *start,
	struct timespec *end)
{
	return end->tv_sec - start->tv_sec +
		(double)(end->tv_nsec - start->tv_nsec) * 1e-9;
}


void load_benchmark_1(long long looptime){
  struct timespec start, end;
  double time_used, perf;
  constexpr int row = 1024;
  int data_size = row * 16 * sizeof(float);
  // float *array = (float *)malloc(data_size);
  float array[row][16]{};
 
  // int64_t looptime = 10000000;
  auto ldop = ldxy().register_index(0);
//   ldop = ldop.multiple();
//   ldop = ldop.multiple_four();
  AMX_SET();
  //warm up

  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(int i = 0; i < looptime; i++){
    for(int j = 0; j < row; j += 8){
      AMX_LDX(ldop.bind(array[j]));
      AMX_LDX(ldop.bind(array[j + 1]));
      AMX_LDX(ldop.bind(array[j + 2]));
      AMX_LDX(ldop.bind(array[j + 3]));
      AMX_LDX(ldop.bind(array[j + 4]));
      AMX_LDX(ldop.bind(array[j + 5]));
      AMX_LDX(ldop.bind(array[j + 6]));
      AMX_LDX(ldop.bind(array[j + 7]));
      
    }
  }
  AMX_CLR();
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  time_used = get_time(&start, &end);
  std::cout << time_used << std::endl;
  perf = looptime * data_size / (1e9) / (time_used);
  std::cout << "Bandwidth 1: " << perf << " GB/s" << std::endl;
}

void load_benchmark_2(long long looptime){
  struct timespec start, end;
  double time_used, perf;
  constexpr int row = 512;
  int data_size = row * 2 * 16 * sizeof(float);
  // float *array = (float *)malloc(data_size);
  float array[row][16 * 2]{};
 
  // int64_t looptime = 10000000;
  auto ldop = ldxy().register_index(0);
  ldop = ldop.multiple();
//   ldop = ldop.multiple_four();
  AMX_SET();
  //warm up

  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(int i = 0; i < looptime; i++){
    for(int j = 0; j < row; j += 8){
      AMX_LDX(ldop.bind(array[j]));
      AMX_LDX(ldop.bind(array[j + 1]));
      AMX_LDX(ldop.bind(array[j + 2]));
      AMX_LDX(ldop.bind(array[j + 3]));
      AMX_LDX(ldop.bind(array[j + 4]));
      AMX_LDX(ldop.bind(array[j + 5]));
      AMX_LDX(ldop.bind(array[j + 6]));
      AMX_LDX(ldop.bind(array[j + 7]));
      
    }
  }
  AMX_CLR();
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  time_used = get_time(&start, &end);
  std::cout << time_used << std::endl;
  perf = looptime * data_size / (1e9) / (time_used);
  std::cout << "Bandwidth 2: " << perf << " GB/s" << std::endl;
}

void load_benchmark_4(long long looptime){
  struct timespec start, end;
  double time_used, perf;
  constexpr int row = 256;
  int data_size = row * 4 * 16 * sizeof(float);
  // float *array = (float *)malloc(data_size);
  float array[row][16 * 4]{};
 
  // int64_t looptime = 10000000;
  auto ldop = ldxy().register_index(0);
  ldop = ldop.multiple();
  ldop = ldop.multiple_four();
  AMX_SET();
  //warm up

  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(int i = 0; i < looptime; i++){
    for(int j = 0; j < row; j += 8){
      AMX_LDX(ldop.bind(array[j]));
      AMX_LDX(ldop.bind(array[j + 1]));
      AMX_LDX(ldop.bind(array[j + 2]));
      AMX_LDX(ldop.bind(array[j + 3]));
      AMX_LDX(ldop.bind(array[j + 4]));
      AMX_LDX(ldop.bind(array[j + 5]));
      AMX_LDX(ldop.bind(array[j + 6]));
      AMX_LDX(ldop.bind(array[j + 7]));
      
    }
  }
  AMX_CLR();
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  time_used = get_time(&start, &end);
  std::cout << time_used << std::endl;
  perf = looptime * data_size / (1e9) / (time_used);
  std::cout << "Bandwidth 4: " << perf << " GB/s" << std::endl;
}

void fmla32_benchmark_mat(long long looptime){
  fma32 fma32_0, fma32_1, fma32_2, fma32_3;

  
  struct timespec start, end;

  AMX_SET();
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(int i = 0; i < looptime; i++){
      AMX_FMA32(fma32_0);
      AMX_FMA32(fma32_0);
      AMX_FMA32(fma32_0);
      AMX_FMA32(fma32_0);
      AMX_FMA32(fma32_0);
      AMX_FMA32(fma32_0);
      AMX_FMA32(fma32_0);
      AMX_FMA32(fma32_0);
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  AMX_CLR();
  double time_used = get_time(&start, &end);
  std::cout << time_used << std::endl;
  double perf = (double)looptime * (double)(512) * 8 / 1e9 / (time_used);
  std::cout << "fma32 mat: " << perf << " GFlops" << std::endl;
}

void fmla16_benchmark_mat(long long looptime){
  fma16 fma16_0, fma16_1, fma16_2, fma16_3;

  
  struct timespec start, end;

  AMX_SET();
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(int i = 0; i < looptime; i++){
      AMX_FMA16(fma16_0);
      AMX_FMA16(fma16_0);
      AMX_FMA16(fma16_0);
      AMX_FMA16(fma16_0);
      AMX_FMA16(fma16_0);
      AMX_FMA16(fma16_0);
      AMX_FMA16(fma16_0);
      AMX_FMA16(fma16_0);
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  AMX_CLR();
  double time_used = get_time(&start, &end);
  std::cout << time_used << std::endl;
  double perf = (double)looptime * (double)(32 * 32* 2) * 8 / 1e9 / (time_used);
  std::cout << "fma16 mat: " << perf << " GFlops" << std::endl;
}

void fmla64_benchmark_mat(long long looptime){
  fma64 fma64_0, fma64_1, fma64_2, fma64_3;

  
  struct timespec start, end;

  AMX_SET();
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(int i = 0; i < looptime; i++){
      AMX_FMA64(fma64_0);
      AMX_FMA64(fma64_0);
      AMX_FMA64(fma64_0);
      AMX_FMA64(fma64_0);
      AMX_FMA64(fma64_0);
      AMX_FMA64(fma64_0);
      AMX_FMA64(fma64_0);
      AMX_FMA64(fma64_0);
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  AMX_CLR();
  double time_used = get_time(&start, &end);
  std::cout << time_used << std::endl;
  double perf = (double)looptime * (double)(8 * 8 * 2) * 8 / 1e9 / (time_used);
  std::cout << "fma64: " << perf << " GFlops" << std::endl;
}


void fmla32_benchmark_vec(long long looptime){
  fma32 fma32_0, fma32_1, fma32_2, fma32_3;
  fma32_0.vector_mode();
  
  struct timespec start, end;

  AMX_SET();
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(int i = 0; i < looptime; i++){
      AMX_FMA32(fma32_0);
      AMX_FMA32(fma32_0);
      AMX_FMA32(fma32_0);
      AMX_FMA32(fma32_0);
      AMX_FMA32(fma32_0);
      AMX_FMA32(fma32_0);
      AMX_FMA32(fma32_0);
      AMX_FMA32(fma32_0);
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  AMX_CLR();
  double time_used = get_time(&start, &end);
  std::cout << time_used << std::endl;
  double perf = (double)looptime * (double)(16 * 2) * 8 / 1e9 / (time_used);
  std::cout << "fma32 vec: " << perf << " GFlops" << std::endl;
}

void fmla16_benchmark_vec(long long looptime){
  fma16 fma16_0, fma16_1, fma16_2, fma16_3;
  fma16_0.vector_mode();
  
  struct timespec start, end;

  AMX_SET();
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(int i = 0; i < looptime; i++){
      AMX_FMA16(fma16_0);
      AMX_FMA16(fma16_0);
      AMX_FMA16(fma16_0);
      AMX_FMA16(fma16_0);
      AMX_FMA16(fma16_0);
      AMX_FMA16(fma16_0);
      AMX_FMA16(fma16_0);
      AMX_FMA16(fma16_0);
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  AMX_CLR();
  double time_used = get_time(&start, &end);
  std::cout << time_used << std::endl;
  double perf = (double)looptime * (double)(32 * 2) * 8 / 1e9 / (time_used);
  std::cout << "fma16 vec: " << perf << " GFlops" << std::endl;
}

void fmla64_benchmark_vec(long long looptime){
  fma64 fma64_0, fma64_1, fma64_2, fma64_3;

  
  struct timespec start, end;
  fma64_0.vector_mode();
  AMX_SET();
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(int i = 0; i < looptime; i++){
      AMX_FMA64(fma64_0);
      AMX_FMA64(fma64_0);
      AMX_FMA64(fma64_0);
      AMX_FMA64(fma64_0);
      AMX_FMA64(fma64_0);
      AMX_FMA64(fma64_0);
      AMX_FMA64(fma64_0);
      AMX_FMA64(fma64_0);
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  AMX_CLR();
  double time_used = get_time(&start, &end);
  std::cout << time_used << std::endl;
  double perf = (double)looptime * (double)(8 * 2) * 8 / 1e9 / (time_used);
  std::cout << "fma64 vec: " << perf << " GFlops" << std::endl;
}

void matint_i8i8_benchmark(long long looptime){
  matint matint_1;
  matint_1 = matint_1.alu_mode(ifma_alt).dtype_mode(def);

  
  struct timespec start, end;

  AMX_SET();
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(int i = 0; i < looptime; i++){
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  AMX_CLR();
  double time_used = get_time(&start, &end);
  std::cout << time_used << std::endl;
  double perf = (double)looptime * (double)(64 * 64) * 2 * 8 / 1e9 / (time_used);
  std::cout << "matint mat i8i8: " << perf << " Gops" << std::endl;
}

void matint_i8i16_benchmark(long long looptime){
  matint matint_1;
  matint_1 = matint_1.alu_mode(ifma_alt).dtype_mode(c);

  
  struct timespec start, end;

  AMX_SET();
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(int i = 0; i < looptime; i++){
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  AMX_CLR();
  double time_used = get_time(&start, &end);
  std::cout << time_used << std::endl;
  double perf = (double)looptime * (double)(64 * 32) * 2 * 8 / 1e9 / (time_used);
  std::cout << "matint mat i8i16: " << perf << " Gops" << std::endl;
}

void matint_i16i16_benchmark(long long looptime){
  matint matint_1;
  matint_1 = matint_1.alu_mode(ifma).dtype_mode(def);
  struct timespec start, end;

  AMX_SET();
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for(int i = 0; i < looptime; i++){
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
      AMX_MATINT(matint_1);
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  AMX_CLR();
  double time_used = get_time(&start, &end);
  std::cout << time_used << std::endl;
  double perf = (double)looptime * (double)(32 * 32) * 2 * 8 / 1e9 / (time_used);
  std::cout << "matint mat i16i16: " << perf << " Gops" << std::endl;
}