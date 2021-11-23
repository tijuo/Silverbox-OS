use num_traits::PrimInt;

pub trait Align {
    type Boundary;
    type Output;

    fn is_aligned(&self, boundary: Self::Boundary) -> bool;
    fn align(&self, boundary: Self::Boundary) -> Self::Output;
    fn align_next(&self, boundary: Self::Boundary) -> Self::Output;
    fn align_trunc(&self, boundary: Self::Boundary) -> Self::Output;
}

impl<T> Align for T
    where T: PrimInt {
    type Boundary = T;
    type Output = T;

    fn is_aligned(&self, boundary: Self::Boundary) -> bool {
        (*self % boundary) == T::zero()
    }

    fn align(&self, boundary: Self::Boundary) -> Self::Output {
        if !self.is_aligned(boundary) {
            self.align_next(boundary)
        } else {
            *self
        }
    }

    fn align_next(&self, boundary: Self::Boundary) -> Self::Output {
        *self + (boundary - (*self % boundary))
    }

    fn align_trunc(&self, boundary: Self::Boundary) -> Self::Output {
        if !self.is_aligned(boundary) {
            *self - (*self % boundary)
        } else {
            *self
        }
    }
}

#[cfg(test)]
mod test {
    use super::Align;

    #[test]
    fn test_align() {
        let x = 4095;
        let x2 = 8192;
        let x3 = 0;
        let page_size = 4096;

        assert_eq!(&x.align(page_size), &4096);
        assert_eq!(&x2.align(page_size), &x2);
        assert_eq!(&x3.align(page_size), &x3);
    }

    #[test]
    fn test_is_aligned() {
        let x = 4095;
        let x2 = 8192;
        let x3 = 0;
        let page_size = 4096;

        assert!(!x.is_aligned(page_size));
        assert!(x3.is_aligned(page_size));
        assert!(x2.is_aligned(page_size));

    }

    #[test]
    fn test_align_trunc() {
        let x = 4095;
        let x2 = 8192;
        let x3 = 0;
        let page_size = 4096;

        assert_eq!(&x.align_trunc(page_size), &0);
        assert_eq!(&x2.align_trunc(page_size), &x2);
    }
}
