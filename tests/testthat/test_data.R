context("Test outbreaker data and settings")


## test data ##
test_that("test: data are processed fine", {
  ## skip on CRAN
  skip_on_cran()


  ## get data
  x <- fake_outbreak
  out <- outbreaker_data(dates = x$onset, dna = x$dna, w_dens = x$w)
  out_nodna <- outbreaker_data(dates = x$onset, w_dens = x$w)

  ## check output
  expect_is(out, "list")
  expect_is(out$D, "matrix")
  expect_equal(out$max_range, 11)
  expect_equal(out_nodna$L, 0)
  expect_equal(out$L, 1e4)
  expect_equal(out$w_dens, out$f_dens)
  expect_equal(out$log_w_dens[1,], out$log_f_dens)
  expect_error(outbreaker_data(dates = 1, w_dens = c(0,-1)),
               "w_dens has negative entries")

  expect_error(outbreaker_data(dates = 1, w_dens = c(0,1), f_dens = c(0,-1)),
               "f_dens has negative entries")

  wrong_lab_dna <- x$dna
  rownames(wrong_lab_dna) <- paste0("host_", seq_len(nrow(wrong_lab_dna)))
  expect_error(outbreaker_data(dates = x$onset, dna = wrong_lab_dna, w_dens = x$w),
               "DNA sequence labels don't match case ids")


})







test_that("outbreaker_data accepts epicontacts and case labelling", {

  ## skip on CRAN
  skip_on_cran()

  ## outbreaker time, ctd, no DNA ##
  ## analysis
  set.seed(1)

  ## get data
  x <- fake_outbreak

  ids_char <- replicate(length(fake_outbreak$sample),
                        paste(sample(letters, 5, TRUE), collapse = ""))

  ids_num <- sample.int(1000, length(fake_outbreak$sample), FALSE)

  ## check for character and numeric ids
  for(ids in list(ids_char, ids_num)) {

    ## make epi_contacts object
    tTree <- data.frame(i = ids[x$ances],
                        j = ids[1:length(x$ances)])
    ctd <- sim_ctd(tTree, eps = 0.9, lambda = 0.1)
    epi_c <- suppressWarnings(epicontacts::make_epicontacts(linelist = data.frame(id = ids),
                                                            contacts = ctd,
                                                            directed = TRUE))

    data <- outbreaker_data(dates = x$onset,
                            dna = x$dna,
                            ctd = epi_c,
                            w_dens = x$w)

    ## test recursiveness
    data <- outbreaker_data(data = data)

    ## check correct contacts are labelled as 1 in matrix
    ctd_ind <- apply(ctd, 2, match, ids)
    expect_equal(rep(1, nrow(ctd)), data$contacts[ctd_ind[,c(2, 1)]])
    expect_equal(rep(0, nrow(ctd)), data$contacts[ctd_ind])

    ## check directionality is being passed
    config <- create_config(data = data)

    ## check ids are carried through
    expect_equal(data$ids, epi_c$linelist$id)

    ## make sure directionality is carried through
    expect_true(config$ctd_directed)


    ## case labelling via dates
    dates <- x$onset
    names(dates) <- ids

    data <- outbreaker_data(dates = dates,
                            dna = x$dna,
                            ctd = ctd,
                            w_dens = x$w)

    ## test recursiveness
    data <- outbreaker_data(data = data)

    ## check direcionality working
    config <- create_config(ctd_directed = TRUE,
                            data = data)

    ## check contact numbers are being updated
    data <- add_convolutions(data, config)

    ## check ids are carried through
    expect_equal(data$ids, as.character(ids))

    ## make sure directionality is carried through
    expect_true(config$ctd_directed)

    ## check the number of contacts are correct
    expect_equal(nrow(ctd), sum(data$contacts))

    ## toggle directionality
    data <- outbreaker_data(dates = dates,
                            dna = x$dna,
                            ctd = ctd,
                            w_dens = x$w)

    data <- outbreaker_data(data = data)

    config <- create_config(ctd_directed = FALSE)

    data <- add_convolutions(data, config)

    ## check the number of contacts are correct
    expect_equal(2*nrow(ctd), sum(data$contacts))

    ## check correct contacts are labelled as 1 in matrix
    ctd_ind <- apply(ctd, 2, match, ids)
    ctd_ind <- rbind(ctd_ind, ctd_ind[,c(2, 1)])
    expect_equal(rep(1, 2*nrow(ctd)), data$contacts[ctd_ind])

    ## make sure directionality is carried through
    expect_false(config$ctd_directed)

    ## identify non-matching labels
    wrong_dna <- x$dna
    rownames(wrong_dna) <- 1:length(x$onset)

    expect_error(data <- outbreaker_data(dates = dates,
                                         dna = wrong_dna,
                                         ctd = ctd,
                                         w_dens = x$w,
                                         ctd_directed = TRUE),
                 "DNA sequence labels don't match case ids")

  }

})



