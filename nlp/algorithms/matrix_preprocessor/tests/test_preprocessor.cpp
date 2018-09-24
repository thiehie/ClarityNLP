// Copyright 2014 Georgia Institute of Technology
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <string>
#include <algorithm>
#include "utils.hpp"
#include "preprocess.hpp"
#include "preprocess_common.hpp"
#include "sparse_matrix_decl.hpp"
#include "sparse_matrix_impl.hpp"
#include "sparse_matrix_io.hpp"
#include "term_frequency_matrix.hpp"
#include "score.hpp"

using std::cout;
using std::cerr;
using std::endl;

//-----------------------------------------------------------------------------
bool TestPreprocessor(const std::string& data_dir)
{
    const std::string BASE_DIR = EnsureTrailingPathSep(data_dir);

    // A list of folders containing "matrix.mtx" and "reduced_matrix.mtx", the
    // latter having been generated by the Matlab preprocessing script.
    std::vector<std::string> dir_list;

    //-------------------------------------------------------------------------
    //
    //                    SET PATHS TO TEST MATRICES
    //
    //-------------------------------------------------------------------------

    //
    // The first matrix will be processed with boolean_mode == false, since it
    // contains tf counts for the NMF_20news_input_tf data set.
    //

    // 1. path to the matrix containing tf counts
    dir_list.push_back(BASE_DIR + std::string("NMF_20news_input_tf/"));

    // 2. path to the Matlab result of processing matrix 1
    dir_list.push_back(BASE_DIR + std::string("NMF_20news_input_tf_reduced/"));

    //
    // The remaining matrices will be processed with boolean_mode == false,
    // since the matrices checked into the repo contain tf-idf SCORES and not
    // raw tf COUNTS.  The preprocessor will read these matrices and convert
    // all nonzeros to '1'.
    //

    // 3. path to the matrix containing tf-idf scores
    dir_list.push_back(BASE_DIR + std::string("NMF_20news_input/"));

    // 4.  path to the Matlab result of processing matrix 3 in boolean mode
    dir_list.push_back(BASE_DIR + std::string("NMF_20news_input_reduced/"));

    // 5.  path to the matrix containing tf-idf scores
    dir_list.push_back(BASE_DIR + std::string("NMF_wikipedia_big_input/"));

    // 6.  path to the Matlab result of processing matrix 5 in boolean mode
    dir_list.push_back(BASE_DIR + std::string("NMF_wikipedia_big_input_reduced/"));

    // 7.  path to the matrix containing tf-idf scores
    dir_list.push_back(BASE_DIR + std::string("wikipedia_800K/"));

    // 8.  path to the Matlab result of processing matrix 7 in boolean mode
    dir_list.push_back(BASE_DIR + std::string("wikipedia_800K_reduced/"));

    //-------------------------------------------------------------------------
    //
    //                              RUN TESTS
    //
    //-------------------------------------------------------------------------

    std::vector<unsigned int> term_indices;
    std::vector<unsigned int> doc_indices;
    const unsigned int MAX_ITER = 1000;
    const unsigned int MIN_DOCS_PER_TERM = 3;
    const unsigned int MIN_TERMS_PER_DOC = 5;

    std::vector<std::string> matrixfiles;
    matrixfiles.push_back("matrix.mtx");
    matrixfiles.push_back("reduced_matrix.mtx");

    SparseMatrix<double> A[2];
    unsigned int height[2], width[2], nz[2];
    std::vector<double> scores;

    // save the current directory - will be restored at exit
    std::string save_dir;
    if (!GetCurrentDirectory(save_dir))
    {
        cout << "test_preprocessor: error - could not save the current directory." << endl;
        return false;
    }

    cout << "\nRunning preprocessor tests...\n" << endl;

    unsigned int dir_count = dir_list.size();
    for (unsigned int i=0; i != dir_count; i += 2)
    {
        bool boolean_mode = (i >= 2);

        std::string dir = dir_list[i];
        if (!SetCurrentDirectory(dir))
        {
            cerr << "test_preprocessor: error - could not cd to " << dir << endl;
            SetCurrentDirectory(save_dir);
            return false;
        }

        // load the matrix.mtx file
        cout << "Loading unprocessed matrix in folder: " << dir << endl;
        if (!LoadMatrixMarketFile(matrixfiles[0], A[0], height[0], width[0], nz[0]))
        {
            cerr << "test_preprocessor: error - could not load matrixfile " 
                 << matrixfiles[0] << endl;
            continue;
        }

        // initialize a TermFrequencyMatrix from it
        TermFrequencyMatrix M0(A[0].Height(),
                               A[0].Width(),
                               A[0].Size(),
                               A[0].LockedColBuffer(),
                               A[0].LockedRowBuffer(),
                               A[0].LockedDataBuffer(),
                               boolean_mode);

        // run the C++ preprocessor
        term_indices.resize(height[0]);
        doc_indices.resize(width[0]);
        bool ok = preprocess_tf(M0, term_indices, doc_indices, //scores,
                                MAX_ITER, MIN_DOCS_PER_TERM, MIN_TERMS_PER_DOC);
        ComputeTfIdf(M0, scores);
        if (!ok)
        {
            cout << "test_preprocessor: error - preprocess failed." << endl;
            SetCurrentDirectory(save_dir);
            return false;
        }

        // load the Matlab-preprocessed file
        dir = dir_list[i+1];
        if (!SetCurrentDirectory(dir))
        {
            cerr << "test_preprocessor: error - could not cd to " << dir << endl;
            SetCurrentDirectory(save_dir);
            return false;
        }
        
        cout << "Loading Matlab result matrix from folder: " << dir << endl;
        if (!LoadMatrixMarketFile(matrixfiles[1], A[1], height[1], width[1], nz[1]))
        {
            cerr << "test_preprocessor: error - could not load matrixfile "
                 << matrixfiles[1] << endl;
            continue;
        }
        
        // initialize a TermFrequencyMatrix from the Matlab result, but
        // don't do any preprocessing on it
        TermFrequencyMatrix M1(A[1].Height(),
                               A[1].Width(),
                               A[1].Size(),
                               A[1].LockedColBuffer(),
                               A[1].LockedRowBuffer(),
                               A[1].LockedDataBuffer(),
                               boolean_mode);
        
        // this file has already been preprocessed

        // compare the matrices
        if (M0.CompareAsBoolean(M1))
        {            
            // compute the Frobenius norm of the difference between the scores
            // for M0 and the loaded data values for A[1]
            
            cout << "\n\tterm-frequency matrices at indices "
                 << i << " and " << i+1
                 << " have an identical pattern of nonzeros " << endl;

            const double* data1 = A[1].LockedDataBuffer();
            unsigned int size = M1.Size();
            if (size != A[1].Size())
            {
                cerr << "test_preprocessor: error - unexpected nonzero count "
                     << "for matrices at indices " << i << " and " << i+1
                     << endl;
                break;
            }

            // compute sum of squared differences
            double sum_sq = 0.0;
            for (unsigned int s=0; s != size; ++s)
            {
                double diff = scores[s] - data1[s];
                sum_sq += (diff*diff);
            }

            cout << "\tFrobenius norm of difference matrix: " << sqrt(sum_sq) << endl;
            cout << endl;
        }
        else
        {
            cout << "\n\tterm-frequency matrices at indices ";
            cout << i << " and " << i+1
                 << " do not have an identical pattern of nonzeros" << endl;

            // Are the matrices a permutation of each other's columns?
            // Conduct two tests to find out.  First find out if the columns
            // contain an identical distribution of nonzero row indices. Do 
            // this by hashing the row indices in each column, sorting the
            // hashes, and comparing 1-1.  In boolean mode, the hashes should
            // be unique.  In term-frequency mode, they may not be.

            // Then compute the norms of each column, sort them, and compare
            // the norms 1-1.  
            
            unsigned int width = M0.Width();
            if (M1.Width() != width)
            {
                cout << "test_preprocessor: sparse binary matrices at indices "
                     << i << " and " << i+1
                     << " have unequal widths." << endl;
                continue;
            }

            std::vector<IndexedData> hash0(width);
            std::vector<IndexedData> hash1(width);

            // extract row data from M0
            const TFData* tf_data0 = M0.LockedTFDataBuffer();
            std::vector<unsigned int> rows0(M0.Size());
            for (unsigned int s=0; s != M0.Size(); ++s)
                rows0[s] = tf_data0[s].row;

            // hash the row indices of both matrices and sort the results
            HashColsSpooky(width, M0.ColBuffer(), &rows0[0], hash0);
            HashColsSpooky(width, A[1].ColBuffer(), A[1].RowBuffer(), hash1);

            std::sort(hash0.begin(), hash0.begin() + width,
                      [](const IndexedData& d1, const IndexedData& d2)
                      {
                          return d1.value < d2.value;
                      });

            std::sort(hash1.begin(), hash1.begin() + width,
                      [](const IndexedData& d1, const IndexedData& d2)
                      {
                          return d1.value < d2.value;
                      }); 

            // Compare the sorted hashes for equality.
            bool permuted = true;
            for (unsigned int c=0; c != width; ++c)
            {
                if (hash0[c].value != hash1[c].value)
                {
                    cout << "\ntest_preprocessor: matrices at indices "
                         << i << " and " << i+1
                         << " are NOT column permutations of each other" << endl;
                    permuted = false;
                    break;
                }
            }

            // For each matrix, compute the Frobenius norm of each column, sort
            // the norms, and compare 1-1.
            std::vector<double> norms0(width);
            std::vector<double> norms1(width);

            // first matrix
            const unsigned int* cols0 = M0.LockedColBuffer();
            for (unsigned int c=0; c != width; ++c)
            {
                unsigned int start = cols0[c];
                unsigned int end   = cols0[c+1];
                
                double sum_sq = 0.0;
                for (unsigned int offset=start; offset != end; ++offset)
                    sum_sq += scores[offset]*scores[offset];
                norms0[c] = sqrt(sum_sq);
            }

            // second matrix
            const unsigned int* cols1 = A[1].LockedColBuffer();
            const double*       data1 = A[1].LockedDataBuffer();
            for (unsigned int c=0; c != width; ++c)
            {
                unsigned int start = cols1[c];
                unsigned int end   = cols1[c+1];

                double sum_sq = 0.0;
                for (unsigned int offset=start; offset != end; ++offset)
                    sum_sq += data1[offset]*data1[offset];
                norms1[c] = sqrt(sum_sq);
            }

            // sort both arrays of norms
            std::sort(norms0.begin(), norms0.end());
            std::sort(norms1.begin(), norms1.end());

            // compare 1-1
            double sum_sq = 0.0;
            for (unsigned int c=0; c != width; ++c)
            {
                double diff = norms0[c] - norms1[c];
                sum_sq += (diff*diff);
            }
            
            if (permuted)
            {
                cout << "\tbut they ARE column permutations of each other";
                cout << endl;
            }

            cout << "\tRMS error between column norms: " << sqrt(sum_sq/width) << endl;
            cout << endl;
        }
    }

    SetCurrentDirectory(save_dir);
    return true;
}