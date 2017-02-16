
#include <Rcpp.h>



// IMPORTANT: ON INDEXING VECTORS AND ANCESTRIES

// Most of the functions implemented here are susceptible to be called from R
// via Rcpp, and are therefore treated as interfaces. This causes a number of
// headaches when using indices of cases defined in R (1:N) to refer to elements
// in Rcpp / Cpp vectors (0:N-1). By convention, we store all data on the
// original scale (1:N), and modify indices whenever accessing elements of
// vectors. In other words, in an expression like 'alpha[j]', 'j' should always
// be on the internal scale (0:N-1).

// In all these functions, 'SEXP i' is an optional vector of case indices, on
// the 1:N scale.





// ---------------------------

//   This function returns a vector of indices of cases which could be infector
//   of 'i' (i.e., their infection dates preceed that of 'i'). Only tricky bit
//   here is keep in mind that 't_inf' is indexed from 0 to N-1, while 'i' and
//   'alpha' (ancestors) are values from 1 to N.

//   Original R code:

// are.possible.alpha <- function(t_inf, i) {
//     if (length(i)>1) {
//         stop("i has a length > 1")
//     }
//     if (any(t_inf[i]==min(t_inf))) {
//         return(NA)
//     }
//     return(which(t_inf < t_inf[i[1]]))
// }

// [[Rcpp::export()]]
std::vector<int> cpp_are_possible_ancestors(Rcpp::IntegerVector t_inf, size_t i) {
  size_t n = t_inf.size();
  std::vector<int> out;
  out.reserve(n);
  for (size_t j = 0; j < n; j++) {
    if (t_inf[j] < t_inf[i-1]) { // offset
      out.push_back(j+1); // +1 needed for range 1 ... N
    }
  }
  return out;
}





// ---------------------------

//  This function samples a single value from a vector of integers.

// [[Rcpp::export()]]
size_t cpp_sample1(std::vector<int> x) {
  if (x.size() < 1) {
    Rcpp::Rcerr << "Trying to sample from empty vector" << std::endl;
    Rcpp::stop("Trying to sample from empty vector");
  }

  return x[unif_rand() * x.size()];
}




// ---------------------------

//    This function choose a possible infector for case 'i'; 'i' is on the scale
//    1:N

//    Original R version:

// .choose.possible.alpha <- function(t_inf, i) {
//     return(sample(are.possible.alpha(t_inf=t_inf, i=i), 1))
// }

// [[Rcpp::export()]]
size_t cpp_pick_possible_ancestor(Rcpp::IntegerVector t_inf, size_t i) {
  return cpp_sample1(cpp_are_possible_ancestors(t_inf, i));
}






// ---------------------------

// This function returns the descendents of a given case 'i' in the current
// ancestries; 'i' is on the scale 1:N. The output is also on the scale 1:N.

// Original R version:

// find.descendents <- function(param, i) {
//   ## find descendents
//     which(param.current$alpha==i)
//  }

// [[Rcpp::export()]]
Rcpp::IntegerVector cpp_find_descendents(Rcpp::IntegerVector alpha, size_t i) {
  size_t counter = 0, n = 0;

  // determine size of output vector and create it
  for (size_t j = 0; j < alpha.size(); j++) {
    if (alpha[j] == i) n++;
  }
  
  Rcpp::IntegerVector out(n);

  // fill in output vector
  for (size_t j = 0; j < alpha.size(); j++) {
    if (alpha[j] == i) {
      out[counter++] = j + 1; // offset
    }
  }
  return out;
}







// ---------------------------

// This function returns a vector of indices of cases which are 'local' to a
// case 'i'. Locality is defined as the following set of cases:

// - 'i'
// - the descendents of 'i'
// - 'alpha[i-1]'
// - the descendents of 'alpha[i]' (excluding 'i')

// where 'alpha' is a IntegerVector storing ancestries. Note that 'i' and
// 'alpha' are on the scale 1:N. 

// [[Rcpp::export()]]
Rcpp::IntegerVector cpp_find_local_cases(Rcpp::IntegerVector alpha, size_t i) {
  // determine descendents of 'i':
  Rcpp::IntegerVector desc_i = cpp_find_descendents(alpha, i);
  size_t n = desc_i.size() + 1; // +1 is to count 'i' itself
  
  // determine descendents of 'alpha[i]':
  Rcpp::IntegerVector desc_alpha_i = cpp_find_descendents(alpha,
							  (size_t) alpha[i-1]);
  if (alpha[i-1] != NA_INTEGER) {
    n += desc_alpha_i.size();
  }

  // create output
  Rcpp::IntegerVector out(n);
  size_t counter = 0;

  // 'i'
  out[counter++] = i;

  // 'descendents of 'i'
  for (size_t j = 0; j < desc_i.size(); j++) {
    out[counter++] = desc_i[j];
  }
  
  if (alpha[i-1] != NA_INTEGER) {
    // alpha[i-1] ...
    out[counter++] = alpha[i-1];
    
    // ... and its descendents
    for (size_t j = 0; j < desc_alpha_i.size(); j++) {
      if ( desc_alpha_i[j] != i) {
	out[counter++] = desc_alpha_i[j];
      }
    }
  }

  return out;
}








// ---------------------------

// This function swaps cases in a transmission tree. The focus case is 'i', and
// is swapped with its ancestor 'x=alpha[i-1]'. In other words the change is
// from: x -> i to i -> x
// Involved changes are:

// - descendents of 'i' become descendents of 'x'
// - descendents of 'x' become descendents of 'i'
// - the infector if 'i' becomes the infector of 'x' (i.e. alpha[x-1])
// - the infector if 'x' becomes 'i'
// - infection time of 'i' becomes that of 'x'
// - infection time of 'x' becomes that of 'i'

// Note on indexing: 'i', 'x', and values of alpha are on the scale 1:N. The
// function's output is a list with new alpha and t_inf.

// Note on forbidden swaps: two types of swaps are excluded:
// - 'i' is imported, so that 'alpha[i-1]' is NA_INTEGER
// - 'x' is imported, so that 'alpha[x-1]' is NA_INTEGER

// [[Rcpp::export()]]
Rcpp::List cpp_swap_cases(Rcpp::List param, size_t i) {
  Rcpp::IntegerVector alpha_in = param["alpha"];
  Rcpp::IntegerVector t_inf_in = param["t_inf"];
  Rcpp::IntegerVector alpha_out = clone(alpha_in);
  Rcpp::IntegerVector t_inf_out = clone(t_inf_in);
  Rcpp::List out;
  out["alpha"] = alpha_out;
  out["t_inf"] = t_inf_out;
      
  size_t N = alpha_in.size();
  
  // escape if the case is imported, i.e. alpha[i-1] is NA
  
  if (alpha_in[i-1] == NA_INTEGER) {
    return out;
  }


  // escape if ancestor of the case is imported, i.e. alpha[x-1] is NA
  
  size_t x = (size_t) alpha_in[i-1];
  if (alpha_in[x-1] == NA_INTEGER) {
    return out;
  }
  
 
  // replace ancestries:
  // - descendents of 'i' become descendents of 'x'
  // - descendents of 'x' become descendents of 'i'

  for (size_t j = 0; j < N; j++) {
    if (alpha_in[j] == i) {
      alpha_out[j] = x;
    } else if (alpha_in[j] == x) {
      alpha_out[j] = i;
    }
  }


  // the ancestor of 'i' becomes an ancestor of 'x'

  alpha_out[i-1] = alpha_in[x-1];

  
  // 'i' is now the ancestor of 'x'
  alpha_out[x-1] = i;
  

  // swap infections times of 'i' and 'x'
  t_inf_out[i-1] =   t_inf_in[x-1];
  t_inf_out[x-1] =   t_inf_in[i-1];


  return out;
}






// ---------------------------

// This function returns the number of mutations between two cases from a 'data'
// object. It uses the indexing of cases in the DNA matrix to ensure
// correspondance between cases and their sequences (not all cases may have a
// sequence). 

// i and j are indices of cases on the scale 1:N; note that the vectors and
// matrices are indexed on 0:(N-1).

// [[Rcpp::export()]]
size_t cpp_get_n_mutations(Rcpp::List data, size_t i, size_t j) {
  Rcpp::LogicalVector has_dna = data["has_dna"];
  Rcpp::IntegerVector id_in_dna = data["id_in_dna"];
  Rcpp::IntegerMatrix D = data["D"];

  
  // Ideally we should return NA_integer here, but then the type of the function
  // should be a Rcpp::IntegerVector, which would complicate things. The second
  // best thing we can do here really is to issue an error when trying to get
  // number of mutations between cases with missing sequences.
  
  if (!(has_dna[i-1] && has_dna[j-1])) {
    Rcpp::stop("Trying to get genetic distances between missing sequences.");
  } 

  size_t out = D(id_in_dna[i-1] - 1, id_in_dna[j-1] - 1);

  return out;
  
}
